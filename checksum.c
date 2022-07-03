#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Program to bruteforce Devolution's anti-tamper checksum by fiddling the 4-5 bytes at the end of it
// As far as I can tell I've never needed more than 34 bits to bruteforce the checksum
// Set N_THREADS to the number of threads you want this to use

typedef union {
  struct __attribute__((packed)) {
    uint16_t cpu_to_dsp_h; // 0x400
    uint16_t cpu_to_dsp_l; // 0x401
    uint16_t unk_0;        // 0x402
    uint16_t unlock_len;   // 0x403
    uint16_t aram_addr_h;  // 0x404
    uint16_t aram_addr_l;  // 0x405
    uint16_t dsp_to_cpu_h; // 0x406
    uint16_t dsp_to_cpu_l; // 0x407
  };
  uint16_t work[0x12];
} data_u;

typedef struct {
   uint8_t *buffer;
   uint32_t addr;
   uint32_t size;
   uint16_t gain;
   uint16_t a1;
   uint16_t a2;
   uint16_t yn1;
   uint16_t yn2;
} accel_s;

void card4_subfunc(data_u *data, uint32_t acc0)
{
  uint32_t acc1;
  acc0 = acc0 + (data->work[0x0e] << 16 | data->work[0x0f]);
  acc1 = (acc0 & 0xFFFF) << 16;
  acc1 ^= data->work[0x0d] << 16;
  acc1 >>= 16;
  acc1 = (acc0 & 0xFFFF0000) | (acc1 & 0xFFFF);
  acc1 ^= data->work[0x0c] << 16; acc0 = (acc0 & 0xFFFF0000) | (data->work[0x11]);
  acc0++;
  data->work[0x11] = acc0 & 0xFFFF;
  acc0 += data->work[0x10];
  acc0 &= 0x1f; acc0 <<= 16;
  uint16_t shift0 = acc0 >> 16;
  acc0 = ((acc0 & 0xFFFF0000) + (int32_t)((uint32_t)-0x20 << 16)) | (acc0 & 0xFFFF);
  uint16_t shift1 = acc0 >> 16; acc0 = acc1;
  if (shift0 & 64)
  {
    if (shift0 & 63)
    {
      uint16_t true_shift = 64 - (shift0 & 0x3f);
      if (true_shift >= 32)
      {
        acc0 = 0;
      }
      else
      {
        acc0 >>= true_shift;
      }
    }
  }
  else
  {
    uint16_t true_shift = shift0 & 0x3f;
    if (true_shift >= 32)
    {
      acc0 = 0;
    }
    else
    {
      acc0 <<= shift0 & 0x3f;
    }
  }
  if (shift1 & 64)
  {
    if (shift1 & 63)
    {
      uint16_t true_shift = 64 - (shift1 & 0x3f);
      if (true_shift >= 32)
      {
        acc1 = 0;
      }
      else
      {
        acc1 >>= true_shift;
      }
    }
  }
  else
  {
    uint16_t true_shift = shift1 & 0x3f;
    if (true_shift >= 32)
    {
      acc1 = 0;
    }
    else
    {
      acc1 <<= shift1 & 0x3f;
    }
  } 
  acc0 += acc1;
  acc0 = acc0 + (data->work[0x0a] << 16 | data->work[0x0b]);
  data->work[0x0b] = acc0 & 0xFFFF;
  data->work[0x0a] = acc0 >> 16;
  acc0 ^= data->work[0x08] << 16;
  acc0 ^= data->work[0x0e] << 16;
  data->work[0x0c] = acc0 >> 16; acc0 <<= 16;
  acc0 ^= data->work[0x09] << 16;
  acc0 ^= data->work[0x0f] << 16;
  data->work[0x0d] = acc0 >> 16;
  acc1 = ((~data->work[0x09] & 0xFFFF) << 16) | (acc1 & 0xFFFF);
  acc1 &= (acc0 & 0xFFFF0000) | 0xFFFF;
  acc0 = (data->work[0x09] << 16) | (acc0 & 0xFFFF);
  acc0 &= (data->work[0x0b] << 16) | 0xFFFF;
  acc0 |= acc1 & 0xFFFF0000; acc1 = ((~data->work[0x08] & 0xFFFF) << 16) | (acc1 & 0xFFFF);
  acc0 >>= 16;
  acc1 &= (data->work[0x0c] << 16) | 0xFFFF; acc0 = (data->work[0x08] << 16) | (acc0 & 0xFFFF);
  acc0 &= (data->work[0x0a] << 16) | 0xFFFF;
  acc0 |= (acc1 & 0xFFFF0000); acc1 = (data->work[0x0e] << 16) | (acc1 & 0xFFFF);
  acc1 = (acc1 & 0xFFFF0000) | (data->work[0x0f]);
  acc0 += acc1;
  data->work[0x0e] = (acc0 >> 16) & 0xFFFF;
  data->work[0x0f] = acc0 & 0xFFFF;
}

uint16_t read_accel(accel_s *accel)
{
  uint16_t data;
  if (accel->addr & 1)
  {
    data = accel->buffer[accel->addr/2] & 0xF;
  }
  else
  {
    data = accel->buffer[accel->addr/2] >> 4;
  }
  accel->addr++;
  data = (data * accel->gain + accel->yn1 * accel->a1 + accel->yn2 * accel->a2);
  accel->yn2 = accel->yn1;
  accel->yn1 = data;
  return data;
}

void card4_part3(data_u *data, accel_s *accel, uint32_t bytes_to_read)
{
  uint32_t new = ((data->work[0x08] << 16) | data->work[0x09]) + ((read_accel(accel) << 16) | read_accel(accel));
  data->work[0x08] = new >> 16;
  data->work[0x09] = new & 0xFFFF;
  for (int i = 0; i < bytes_to_read - 1; i++)
  {
    card4_subfunc(data, new);
    new = ((data->work[0x08] << 16) | data->work[0x09]) + ((read_accel(accel) << 16) | read_accel(accel));
    data->work[0x08] = new >> 16;
    data->work[0x09] = new & 0xFFFF;
  }
  card4_subfunc(data, new);
}

void card4_part2(data_u *data, accel_s *accel, uint32_t acc0, uint16_t byte_len)
{
  uint32_t acc1 = acc0;
  acc0 = (int32_t)acc0 + (int32_t)0x4ea21e71;
  data->work[0x08] = acc0 >> 16;
  data->work[0x09] = acc0 & 0xFFFF;
  data->work[0x0a] = 0xcc0a;
  data->work[0x0b] = 0x144b;
  data->work[0x0c] = 0xf541;
  data->work[0x0d] = 0x878d;
  data->work[0x0e] = 0xa3bc;
  acc1 += 3; data->work[0x0f] = 0x64e4;
  data->work[0x10] = acc1 & 0xFFFF;
  data->work[0x11] = 0;
  accel->addr = 0;
  accel->gain = 0xfc82;
  accel->a1 = 0x978;
  accel->a2 = 0xe541;
  accel->yn1 = 0x0000;
  accel->yn2 = 0x0000;
  card4_part3(data, accel, byte_len);
}

typedef struct {
  uint64_t id;
  uint32_t n;
  uint32_t bytes;
  uint32_t payload_len;
  data_u *data_bak;
  accel_s *accel_bak;
} parallel_info;

atomic_bool found = ATOMIC_VAR_INIT(false);
uint8_t patch_buf[8];

void* bruteforce_func(void* usrdata)
{
  data_u ldata;
  accel_s laccel;
  parallel_info *info = (parallel_info*)usrdata;
  uint8_t dumb_buf[8];
  uint32_t magic_other_half = 0x735f0000 + info->payload_len;
  for (uint64_t val = info->id; val < (1ULL << info->bytes * 8); val += info->n)
  {
    memcpy(&ldata, info->data_bak, sizeof(data_u));
    memcpy(&laccel, info->accel_bak, sizeof(accel_s));
    laccel.buffer = &dumb_buf[0];
    laccel.addr = 0;
    for (int o = 0; o < info->bytes; o++)
    {
      laccel.buffer[o] = val >> ((info->bytes*8) - ((o+1)*8));
    }
    card4_part3(&ldata, &laccel, info->bytes);
    uint32_t checksum = (ldata.work[0x0a] << 16) | ldata.work[0x0b];
    // DSP check on the left (lower 16-bits must be 0xFFFF), PPC check on the right
    if (((checksum + info->payload_len) & 0xFFFF) == 0xFFFF && (checksum ^ magic_other_half) == 0x7FFFFFFF)
    {
      printf("Magic value is %d bytes: 0x%08lx (0x%08x 0x%08x)\n", info->bytes, val, info->payload_len, checksum + info->payload_len);
      found = true;
      memcpy(&patch_buf[8-info->bytes], &dumb_buf[0], 8);
      break;
    }
    if (found)
    {
      break;
    }
  }

  return NULL;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf ("checksum requires one file argument\n");
    return 1;
  }
  FILE *ppc_payload = fopen(argv[1], "rb");
  if (ppc_payload == NULL)
  {
    printf("Unable to open payload %s\n", argv[1]);
    return 2;
  }
  fseek(ppc_payload, 0L, SEEK_END);
  uint32_t payload_len = ftell(ppc_payload);
  rewind(ppc_payload);
  data_u data;
  accel_s accel;
  accel.buffer = malloc(payload_len);
  uint64_t jeff;
  if (fread(accel.buffer, payload_len, 1, ppc_payload) != 1)
  {
    printf("Didn't read enough bytes!\n");
    return 3;
  }
  fclose(ppc_payload);
  payload_len = 0x30974;
  card4_part2(&data, &accel, 0, payload_len % 0x10000);
  for (int i = 0; i < payload_len / 0x10000 - 1; i++)
  {
    card4_part3(&data, &accel, 0x10000);
  }
  card4_part3(&data, &accel, 0x10000-5);
  accel.buffer[0x3096F] = 0xFF; // Try to bruteforce with this byte as 0xFF first
  data_u data_bak_5;
  accel_s accel_bak_5;
  memcpy(&data_bak_5, &data, sizeof(data_u));
  memcpy(&accel_bak_5, &accel, sizeof(accel_s));
  card4_part3(&data, &accel, 1);
  data_u data_bak_4;
  accel_s accel_bak_4;
  memcpy(&data_bak_4, &data, sizeof(data_u));
  memcpy(&accel_bak_4, &accel, sizeof(accel_s));

  // Bruteforce the checksum
#ifndef N_THREADS
#define N_THREADS 8
#endif
  memset(&patch_buf[0], 0xFF, 8);
  uint8_t n_bytes = 4;
  pthread_t threads[N_THREADS];
  parallel_info info[N_THREADS];
  for (int i = 0; i < N_THREADS; i++)
  {
    info[i].id = i;
    info[i].n = N_THREADS;
    info[i].bytes = n_bytes;
    info[i].payload_len = payload_len;
    info[i].data_bak = &data_bak_4;
    info[i].accel_bak = &accel_bak_4;
    pthread_create(&threads[i], NULL, bruteforce_func, &info[i]);
  }
  for (int i = 0; i < N_THREADS; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if (!found) {
    n_bytes = 5;
    printf("Doing 5-byte bruteforce. This will take a while.\n");
    pthread_t threads[N_THREADS];
    parallel_info info[N_THREADS];
    for (int i = 0; i < N_THREADS; i++)
    {
      info[i].id = i;
      info[i].n = N_THREADS;
      info[i].bytes = n_bytes;
      info[i].payload_len = payload_len;
      info[i].data_bak = &data_bak_5;
      info[i].accel_bak = &accel_bak_5;
      pthread_create(&threads[i], NULL, bruteforce_func, &info[i]);
    }
    for (int i = 0; i < N_THREADS; i++)
    {
      pthread_join(threads[i], NULL);
    }
  }
  if (!found) {
    printf("Warning: could not find checksum!\n");
  }
  else
  {
    FILE *ppc_payload = fopen(argv[1], "r+b");
    if (ppc_payload == NULL)
    {
      printf("Unable to open payload %s for writing\n", argv[1]);
      return 4;
    }
    fseek(ppc_payload, 0x30974 - 8, SEEK_SET);
    fwrite(&patch_buf[0], 8, 1, ppc_payload);
    fclose(ppc_payload);
  }
  free(accel.buffer);
  return 0;
}
