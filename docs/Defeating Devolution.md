# Defeating Devolution
### Unpacking loader.bin
Devolution features many layers of payload obfuscation, anti-tamper, and funny math to prevent the user from easily defeating its DRM mechanisms. As it turns out, someone else had already unpacked Devolution's first stage loader.bin to obtain the main PowerPC payload back in 2018: https://oct0xor.github.io/2018/05/06/unpacking_devolution/
I cannot emphasize enough how helpful this writeup was, and if nothing else it sped things up by a few weeks.
I first adapted their QEMU Broadway patch and IDA instrumentation to QEMU 7.0 and standalone GDB scripts respectively to simiplify things.
They evidently worked with Devolution r200, so I also adapted the tooling to the last public release of Devolution, r266.
As addresses had shifted around, it was not 100% clear which CRC function call I was supposed to dump, so I just dumped all of the ones it could be to different files, and found the correct one from there.
My GDB adaptation of the IDA script can be found [here](https://github.com/xperia64/unpacking_devolution/blob/master/scripts/devo_unpack_gdb.py). 
### Analyzing the Main Payload
##### Running the Unpacked Payload
Ok, so we got what seems to be the main PowerPC payload, now what?
The initial loader.bin is supposed to copy this payload into MEM2 @ `0x93200000`, so let's try modifying Devolution's sample loader to place the decrypted payload there and boot directly into it.
Just a simple memcpy and flush, and everything still seems to work when we branch to `0x93200000`. Great!
Let's try messing with the version string just for fun, maybe changing "Devolution " to "Evolution  ".
Uh oh. It doesn't boot anymore. I knew this was going way too smoothly.
We've learned two things:
1. The initial loader.bin is *not* needed for Devolution to work and only exists for compression and obfuscation of the main payload, so we can ignore it
2. The main payload has some sort of anti-tamper, because of course it does

##### Finding `main`
Time for some good old-fashioned static analysis.
With the [Ghidra GameCube Loader](https://github.com/Cuyler36/Ghidra-GameCube-Loader), we can load our raw binary payload at `0x93200000` and get to work.
The first thing we see is a short sequence of instructions that end with an `rfi` instruction.
Between this and the initial loader.bin, it seems the author loves using `rfi` for branching.
We can see this `rfi` branches to `0x13200040` (the physical-address-space equivalent of `0x93200040`), so lets mark this instruction as a `CALL_RETURN` in Ghidra to make things look nice, and resume analysis there.
Devolution proceeds to reset/configure a bunch of registers.
Ghidra is apparently not aware of all of [Broadway's SPRs](https://fail0verflow.com/media/files/ppc_750cl.pdf#G6.1103629), so we add comments to the SPRs that don't make sense, denoting the actual register name and what the configuration does.
Most of the register setting is uninteresting, but we make note of the IBAT/DBATs as they are reconfigured here (remember we are running from physical address space).
We can see that MEM1 is now available at `0x80000000`, `0xa0000000` and `0xc0000000` instead of just its usual spot at `0x80000000`, and that MEM2 is available at `0xb0000000` and `0xd0000000` instead of `0x90000000`.
There's some caching and length details that vary among the assignments, and some slightly different IBAT mappings (for example, there's a 128k I-mapping from `0x10000000` to `0xb0000000` and a 1MB I-mapping from `0x13200000 to 0xb3200000`, but the rest of MEM2 is excluded from I-mappings).
Finally, we jump into a very large function at `0xb3200260` which we can think of as `main`.
With the BAT mappings, we load another copy of our payload at `0xb3200000` in Ghidra as there's no real clean way to do mirrors.
The payload at `0xb3200000` will be our main working copy from now on.

##### Funny Strings
Scrolling through this function, we see many variable initializations and many function calls.
One function that catches our eye first is one that takes the Devolution version string as its second argument, with a pointer to some strange bytes as the first argument.
This function calls a second function with some more arguments, and in the second function, we see what appears to be a `printf`-style format string parser.
This seems useful, but what are the other arguments, and what are those strange bytes?
The other arguments appear to be function pointers, one for outputting a character from the `printf`-like function (`putc`?), and the other takes in the strange bytes and returns a normal `char` that gets parsed in the format string parser.
Calling this function `obfuscated_printf_fmt_getter`, we see that it dereferences a byte from the strange string, XORs it with it's address, and then XORs that with the value of some array that's indexed by the previous char in the string and the value of an SPR?
Seems a bit execessive, but let's go back to where that array is initialized.
This array is near the end of the PPC payload, and is filled with zeros.
According to Ghidra, the array is only reinitialized if some conditional bit is set that hasn't been initialized, so let's assume this array is filled with zeros.
At `0xb320099b`, we locate the byte sequence used by this printf function call, `BE EF 97 9E`. Let's XOR each letter with its address and see what we get.
```
0xBE ^ 0x9B = 0x25, '%'
0xEF ^ 0x9C = 0x73, 's'
0x97 ^ 0x9D = 0x0a, '\n'
0x9E ^ 0x9E = 0x00, '\0'
```
As you can see, we end up with a nice null-terminated "%s\n".
By finding the uses of this obfuscated printf, we can locate other obfuscated format strings.
Because decoding these strings by hand is tedious, I've written a [Ghidra python script](https://github.com/xperia64/unpacking_devolution/blob/master/scripts/decode_ghidra.py) which can be pasted into the python console. Just run `deobf()` in the console when you've selected what you think is an obfuscated string in the Listing view.
##### Naming Things
Now that we have the log strings, we can go through each function that calls our obfuscated printf, and try to name it based on what strings it uses.
Doing this, we locate many important functions:
* An `abort_handler` at `0xb320210c`
* What's likely an IOS `ipc_response_handler` at `0xb3207ee4`
* An `exception_handler` at `0xb3202450`
* USB Gecko initialization at `0xb32034cc`
* SD card initialization at `0xb320f5b8`
* USB storage initialization at `0xb321dd90`
* And finally, a very interesting string: `Unable to verify ISO image - .DVV file error`, used in a function that also calls the SD/USB initialization functions; let's call this `important_boot_function`

##### Storage and Loading the DRM Validation Files
Going through `important_boot_function`, we discover code that tries to locate the FAT partition on the storage device, and a custom FAT driver (which only supports 8.3 short names).
We can see boring things like memcard emulation, and then the more interesting part where it tries to load loads the DVV DRM file for the game you've selected.
The code here actually cares about the DVV file being "read-only" and "hidden" (I believe it cares about the archive bit being unset too, but I forgot).
If the file doesn't exist, it attempts to allocate a placeholder, and if any of these steps fail, this is where we get the "DVV file error" message.
Next, Devolution calls an interesting function that takes in part of the DVV buffer, which we soon identify as an IOS_Ioctlv implementation.
This particular implementation again has a custom format string parser, but the use of IOS_Ioctlv is strange as it passes a hardcoded file descriptor of 0x10000.
Thankfully, [someone helpfully documented this value and its usage](https://wiibrew.org/wiki//dev/aes) around two weeks before I started this project. Talk about good timing.
The first 32 bytes of the DVV file are very clearly the first 32 bytes of your GameCube disc, and with this Ioctlv call, we can see that the remaining 224 bytes are encrypted with a fixed 16-byte key located at `0xb3215560`, while the IV is the first 12 bytes of the DVV concatenated with the last 4 bytes of the plaintext part (those magic 4 bytes all GameCube discs have).
I've included a script that dumps the encrypted portion of a DVV [here](https://github.com/xperia64/unpacking_devolution/blob/master/scripts/decrypt_dvv.py) (need to replace the encryption key), and the Wii evidently uses standard AES CBC.
The decrypted DVV section doesn't mean much as there's more high-entropy data after another 32-byte header-like structure.
The second 16-byte struct of the decrypted section seem to be related to the Wiimote validation, with 4 bytes per Wii remote. The values are consistent, but change in order depending on which Wii remote connects first.
The full 256-byte decrypted DVV is then passed to another function which makes absolutely no sense for now, so let's ignore it and move on to naming other functions.

##### Other Functions
Going through and naming more functions by strings or memory address, we find:
* A config struct validator/initializer
* Video register configuration
* A reboot function
* HID initialization
* Bluetooth initialization
* lwIP web server initialization
* Code which runs the game's apploader and hooks up osreport to Devolution's USB Gecko/WiFi logging
* A function which blinks the disc slot LED N times
* A function that encrypts the DVV buffer before writing it to disc

So far, some things are suspiciously absent:
* No sign of that anti-tamper which we know exists
* No code which accesses the Disc Interface

However, there are a few functions which perform reads and writes to addresses specified as offsets to base register `r2`.
Doing a bruteforce Listing search in Ghidra, we discover that `r2` is set in only a couple places:
* Near the start of `main`, `r2 = 0xcc010000`
* In the function that initializes the USB Gecko, `r2 = r2 - 0x8000`

This results in a final `r2` of `0xcc008000` for the remainder of the part we care about.
Unfortunately, Ghidra is not smart enough to assign references to the Wii hardware registers accessed via `r2`, so instead we must go through our Listing search and look for interesting uses of `r2`.

##### Overengineered Disc Validation
This brings us to a function at `0xb3209a10` which does some math on `r2` and throws that into `r13` which is used as a final base register value of `0xcd808000`.
This function uses negative offsets from `r13`, which ultimately shows us that it's poking a mirror of the disc interface at `0xcd806000`.
This function performs many many tests on the disc drive to make sure it's not an ODE, that you're using original discs and not burnt media, that the game ID matches, etc.
This process is similar in concept to these checks in the now-public [Riivolution source](https://github.com/AerialX/rawksd/blob/4ef79e2e234d7c2f19d030c2d9440852b5c73b7a/launcher/source/launcher.cpp#L219)
This seems like a great place to attack if it wasn't for that pesky anti-tamper.

##### LED-as-a-Debugger (LaaD)
After all this, the PowerPC anti-tamper is still nowhere to be found.
We've named pretty much everything we could, and nothing.
I don't have a USB Gecko, so my options for debugging the PowerPC payload on real hardware are very limited, but Devolution loves to blink the LED, so I thought, why not use that to see where the anti-tamper kicks in and halts execution?
By writing a snippet of PowerPC assembly that turns the LED on, then runs a do-nothing loop forever, we can perform a binary search on the boot process and narrow down where execution gets stuck. After many iterations pasting our PowerPC snippet in various places, we narrow down the problem to the first function after initializing the USB Gecko at `0xb3203a28`. We skipped this function in our first static analysis pass as the control flow made no sense.
Additionally at this time, I also determined whether Devolution cares about the whole PowerPC payload or only part of it. It seems that only the first `0x30974` bytes are validated.

##### Oops, I Accidentally Flipped the Endianness of my CPU
As it turns out, the first thing this function does is twiddle with the PowerPC MSRs to enable PowerPC little-endian mode.
That still doesn't explain the funky control flow.... [Oh.](http://web.archive.org/web/20220921155414/https://www.nxp.com/files-static/product/doc/MPCFPE32B.pdf#G9.105725).
PowerPC little-endian mode is incredibly misleading and only messes with how data addresses are interpreted rather than the data itself. Evidently this also applies to instruction fetches, so instructions that would normally run `0, 1, 2, 3` instead run `1, 0, 3, 2` in little-endian mode.
I've written a Ghidra script to flip 4-byte words around to untangle this function [here](https://github.com/xperia64/unpacking_devolution/blob/master/scripts/munge_ghidra.py) (note that you must clear the code first before running it).

##### DSP Anti-Tamper
Now that we have this function untangled, let's see what it's doing.
First it runs some loop doing some funny math on a blob from `0xb32155a0`, and places its output at `0xc1000000`.
Next, it twiddles a bunch of DSP registers.
Referencing a game with debug symbols, we can see that this function is bootloading the DSP, and the blob we did the math on is DSP firmware.
A script to decrypt the 3072-byte blob from `0xb32155a0` is [here](https://github.com/xperia64/unpacking_devolution/blob/master/scripts/decrypt_dsp_payload.py).
There's a lot of back and forth between the PowerPC and the DSP at this stage, some of it standard, some of it strange.
By using [this wonderful Ghidra plugin](https://github.com/Pokechu22/ghidra-gcdsp-lang), we can attempt to analyze what the DSP firmware is doing.
The first set of DSP control register writes automagically DMA's 1024 bytes of the decrypted payload into DSP IRAM, and it then evidently boots into IRAM `0x0000`.
In Devolution's DSP payload, this sets up a few registers, and then sends mail with the value of `0x0000 0080`.
The PPC payload then reads this value back and does more DSP control register twiddling, which appears to cause the DSP to boot from IROM.
This is part of the normal DSP bootloading process, however, Devolution specifies IRAM and DRAM payloads of length 0, and uses `0x0080` from the mailbox as the DSP boot address.
We now assume that the DSP continues execution from IRAM at `0x0080`, and by observing the sequence of mailbox accesses, this seems correct.
The DSP payload stores a constant value of `0x1320 0000` somewhere, so it seems this could be our anti-tamper. Why else would the DSP be referencing the start of the PowerPC payload?

Next, the DSP jumps into the middle of an IROM function once, then jumps further down into the same function multiple times in a loop.
By going through the IROM, we locate a total of 4 functions that all seem to do weird math with slight variation, at IROM `0x854b`, `0x8644`, `0x8726`, and `0x880c`. What the heck are these?
After quite a bit of searching, I located [this comment](https://github.com/dolphin-emu/dolphin/pull/5617#issuecomment-309256012) on a Dolphin PR which indicates that this function is related to memory card unlocking.
For convenience, I've named these functions CARD1 through CARD4, and that Devolution's DSP payload jumps into the middle of CARD4.
This code sets up some random-looking constants, and then configures the DSP's accelerator, setting the current address to 0x13200000 * 2 = 0x26400000. This part of CARD4 loops some number of times.
The number of times to loop is sent by the PowerPC payload to the DSP's mailbox, and I determined that that value is `0xc25d`.
The DSP payload then multiplies that value by 4, and would you look at that, we get `0x30974`.
The first call to the middle of the function loops `0x974` times, and the loop that calls the later part of the function runs `0x30000/0x10000` times, with the loop in the called function running `0x10000` times.
By examining the called function's loop, we can see that it reads the accelerator twice.
Further DSP hardware tests determined that the Accelerator format used is 4-bit, so reading one byte requires 2 reads, and that's also why the accelerator address was multiplied by 2.

Ultimately, this function as used by Devolution does a proprietary obfuscated hash of the PowerPC binary, and this gets mailed to the PowerPC.
The DSP firmware requires that the lower 16-bits of the sum of the hash and the length of the payload equal `0xffff`, and DSP DMA of the rest of the DSP payload fails when the lower 16 bits are any other value.
The PowerPC payload goes a step further and requires that the hash plus payload length XOR'd with `0x735f0000` equals `0x7FFFFFFF`. In other words, the final hash must equal `0x0c9df68b`
Fudging the payload to only satisfy the lower 16 bits and skipping the check for the upper 16 bits on the PowerPC side does not work, so the upper bits evidently matter somewhere else too.
Unless I'm missing some simplified math, it would seem that bruteforcing the checksum to be this specific value was simply part of the build process for devolution.
There are 4 bytes at the end of the checksummmed area which I assumed are used to force the hash to be a specific value. There's another 4 bytes before it which are 0xFFFFFFFF, which I'm assuming are used if the required checksum cannot be achieved in 4 bytes.
After a lot of tedious static and dynamic analysis of the DSP ROM functions using [DSPSpy](https://github.com/dolphin-emu/dolphin/tree/master/Source/DSPSpy), I was able to write a C implementation of the hash function, and wrote a multi-threaded bruteforcer to fix the checksum on a tampered payload, which can be seen [here](https://github.com/xperia64/unpacking_devolution/blob/master/checksum.c).

##### More DSP Shenanigans
After the anti-tamper, it became clear that the DSP was being used as a flat-out security coprocessor.
I went back and located all of the PowerPC functions which ran in little-endian mode, and all of them send information to the DSP using mailed commands:
* Command 0 sends the DVV buffer over to the DSP
* Command 1 is used right before Devolution hands over execution to the actual game, maybe some "request boot" command
* Command 2 sends "short" security metadata
* Command 3 sends disc-related metadata during disc validation
* Command 4 sends the time-base register of the PowerPC (this is done immediately after a successful DSP boot)
* Command 5 sends "long" security metadata
* Commands 6-9 appear to receive Wiimote metadata for Wiimotes 1-4

Command 4 is fun because the time-base register is used to scramble all of the other DRM info (the time gets stored in the DVV so it can decode the DRM info later).
My first real attempt at cracking devolution at this point was chasing a lot of the security metadata down and attempting to zero all of that out.
Devolution uses pretty much every unique identifier a Wii has as part of its DRM, including your NG_ID, NAND encryption key, ECID (thanks Extrems), processor version, Bluetooth MAC address, and probably some other things I missed, as zeroing out this information did not result in transferable DVVs.
Additionally, the ECID reads are scattered throughout other unrelated functions, and the reads themselves are also obfuscated.
Rather than reading the ECID SPRs directly, Devolution intentionally reads invalid SPRs (which always throws an exception on Broadway), and then fixes up the reads to the real ECID SPRs in its exception handler. Lovely.
And of course, the DSP code that receives all this data is even more painful to read than the initial bootload and anti-tamper.

### Cracking Devolution
Ultimately I stopped trying to completely understand how the DRM worked and took a different approach.
The disc drive validation function from before sends two pieces of information about the disc to the DSP, specifically the first 32 bytes of the disc header, and the BCA.
As it turns out, Devolution doesn't care what the BCA data *is* as long as it meets certain conditions.
From Dolphin developer pokechu22's analysis of this part:
```
pokechu22   OK, I looked into the BCA check, and I think I understand it: I assume that JMPR $ar2 (=> 0202) is the "you've messed up" path for the most part.
pokechu22   015b-015f reads the accelerator 0x1a times or 0x34 bytes assuming a 16-bit format. It ors each read value together, and then tests if the result is zero, i.e. all bytes are zero, i.e. the BCA starts with 0x34 zeros.
pokechu22   0161-017a checks if the next value (0x34/0x35) is one of 0x474b = GK, 0x4b47 = KG, 0x4d41 = MA, 0x4d42 = MB, 0x4d44 = MD, 0x4d55 = MU, 0x5044 = PD, 0x504D = PM, or 0x7064 = pd (converting pd to PD if found).
pokechu22   017c-0191 checks if the next value (0x36/0x37) is one of 0x4b44 = KD, 0x4b47 = KG, 0x4d41 = MA, 0x4d43 = MC, 0x5343 = SC, 0x534b = SK, 0x554b = UK, or 0x5544 = UT.
pokechu22   0192-0195 checks if the next value (0x38/0x39) has the top nybbles cleared (i.e. masking with 0xf0f0 results in zero, i.e. the number is of the form 0X0X).
pokechu22   0196-0197 skips one value (0x3a/0x3b) and loads the next.
pokechu22   0198-019a checks that that value (0x3c/0x3d) and 0xfc is zero (i.e. offset 0x3d is 0, 1, 2, or 3). 0x3e/0x3f never seem to be read.
pokechu22   It then writes some stuff into the accelerator (which would clobber the 0x3e/0x3f values?), which only happens if these tests pass, and then jumps to 0202.
pokechu22   If I'm understanding things correctly, a BCA composed of 0x34 zeros followed by 'PDMC' (50 44 4d 43) followed by 8 more zeros would pass.
```

And indeed, Devolution accepts a blank BCA except for `PDMC`.
Since we already have the disc header in memory (as Devolution requires us to pass when we boot it from our launcher), I picked a completely random address in MEM1 (`0xa1466d80`) and placed a buffer with the disc header and our BCA in the modified launcher, and told the disc validation function to pass that to the DSP instead.
After verifying this worked, I wiped all the code in this disc validation function except for the part where it sends our buffer, and it still worked.
For whatever reason placing the buffer into MEM2 *does not* seem to work, but whatever.
Ultimately I probably could've generated a buffer with these contents in the disc validation function, but I chose to instead write a preloader stub and do it there as I needed to anyway so my payload would behave the same as the vanilla loader.bin for use in other launchers like USB Loader GX.
I was not done yet as it turns out there was a little more disc information that was sent over to the DSP after the initial basic metadata checks.
Devolution appears to read various sectors of the disc into a buffer, and the DSP hashes them.
*However*, depending on whether the DVV looks good on the DSP side, Devolution will read these sectors from the emulated disc instead, because it obviously has to if you're playing on a Family Wii or Wii U.
And as it turns out, this is actually done as a nice bit of abstraction with a function pointer table (if DVV good, read from USB/SD storage; else try to read from the real disc).
By forcing this function to always read from USB/SD storage, Devolution passes its own validation.
I quickly wrote a small [PowerPC assembly memcpy program](https://github.com/xperia64/unpacking_devolution/blob/master/patches/preloader.bin) to copy Devolution's payload to 0x93200000 just like the real loader.bin, and also copy the game disc header+dummy BCA to `0x81466d80`, and just like that, I have a drop-in compatible loader.bin without the pesky disc check.

All it took for Devolution's DRM to fall were two minimally-invasive patches and a checksum fix. The rest of the crazy DSP DRM math and other protections were for naught.
What this means is that Family Wii's and  Wii U's can use Devolution without a 1st party Wii remote or disc validation on a backwards compatible Wii. Honestly I was surprised there weren't additional checks to break generating DVVs for these console IDs specifically.
These patches work well on at least backwards-compatible Wii's, but I encountered some issues where games would not boot the first time you ran/validated them on the Wii U (and indeed, I also noticed some instability in games on the Wii when booting them for the first time), so I wrote a 3rd patch which produces a variant of Devolution which generates the DVV file, and exits immediately.
Additionally, the generated DVVs are 100% compatible with an unmodified stock Devolution, so should any other issues appear with my patched version, the user can choose to use their original copy of Devolution r266.

### Ship It
To make these patches relatively accessible while not infringing on the original author's copyright, I have put together Docker containers with all the tools and patches, and written appropriate *nix/Windows scripts to automagically decrypt, patch, and rebuild the binary.
Just grab [gc_devo_src.zip](https://web.archive.org/web/20191026015213/http://www.tueidj.net/gc_devo_src.zip), put it in the appropriate folder, and assuming Docker is set up properly, run `run_docker.sh` or `run_docker.bat`
Is Docker overkill? Absolutely. But the build process is very messy and I don't trust QEMU and gdb to run properly on Windows.
Should I have made prebuilt images and pushed them to DockerHub? Probably but I'm too lazy to make an account. If anyone wants to fork this and provide prebuilt images they're free to do so.

Huge thanks to the Dolphin IRC for being very helpful throughout this and putting up with my questions (I'll try to get around to that DSP PR I swear), and specifically pokechu22 and Extrems.
