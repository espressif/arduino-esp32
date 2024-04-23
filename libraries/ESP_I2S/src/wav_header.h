#pragma once
#include <stdint.h>

/**
 * @brief Header structure for WAV file with only one data chunk
 *
 * @note See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
 *
 * @note Assignment to variables in this struct directly is only possible for little endian architectures
 *       (including Xtensa & RISC-V)
 */

typedef struct {
  char chunk_id[4];                               /*!< Contains the letters "RIFF" in ASCII form */
  uint32_t chunk_size;                            /*!< This is the size of the rest of the chunk following this number */
  char chunk_format[4];                           /*!< Contains the letters "WAVE" */
} __attribute__((packed)) wav_descriptor_chunk_t; /*!< Canonical WAVE format starts with the RIFF header */

typedef struct {
  char subchunk_id[4];                         /*!< Contains the letters "fmt " */
  uint32_t subchunk_size;                      /*!< PCM = 16, This is the size of the rest of the Subchunk which follows this number */
  uint16_t audio_format;                       /*!< PCM = 1, values other than 1 indicate some form of compression */
  uint16_t num_of_channels;                    /*!< Mono = 1, Stereo = 2, etc. */
  uint32_t sample_rate;                        /*!< 8000, 44100, etc. */
  uint32_t byte_rate;                          /*!< ==SampleRate * NumChannels * BitsPerSample s/ 8 */
  uint16_t block_align;                        /*!< ==NumChannels * BitsPerSample / 8 */
  uint16_t bits_per_sample;                    /*!< 8 bits = 8, 16 bits = 16, etc. */
} __attribute__((packed)) pcm_wav_fmt_chunk_t; /*!< The "fmt " subchunk describes the sound data's format */

typedef struct {
  char subchunk_id[4];                             /*!< Contains the letters "fmt " */
  uint32_t subchunk_size;                          /*!< ALAW/MULAW = 18, This is the size of the rest of the Subchunk which follows this number */
  uint16_t audio_format;                           /*!< ALAW = 6, MULAW = 7, values other than 1 indicate some form of compression */
  uint16_t num_of_channels;                        /*!< ALAW/MULAW = 1, Mono = 1, Stereo = 2, etc. */
  uint32_t sample_rate;                            /*!< ALAW/MULAW = 8000, 8000, 44100, etc. */
  uint32_t byte_rate;                              /*!< ALAW/MULAW = 8000, ==SampleRate * NumChannels * BitsPerSample s/ 8 */
  uint16_t block_align;                            /*!< ALAW/MULAW = 1, ==NumChannels * BitsPerSample / 8 */
  uint16_t bits_per_sample;                        /*!< ALAW/MULAW = 8, 8 bits = 8, 16 bits = 16, etc. */
  uint16_t ext_size;                               /*!< ALAW/MULAW = 0, Size of the extension (0 or 22) */
} __attribute__((packed)) non_pcm_wav_fmt_chunk_t; /*!< The "fmt " subchunk describes the sound data's format */

typedef struct {
  char subchunk_id[4];                      /*!< Contains the letters "data" */
  uint32_t subchunk_size;                   /*!< ==NumSamples * NumChannels * BitsPerSample / 8 */
} __attribute__((packed)) wav_data_chunk_t; /*!< The "data" subchunk contains the size of the data and the actual sound */

typedef struct {
  wav_descriptor_chunk_t descriptor_chunk; /*!< Canonical WAVE format starts with the RIFF header */
  pcm_wav_fmt_chunk_t fmt_chunk;           /*!< The "fmt " subchunk describes the sound data's format */
  wav_data_chunk_t data_chunk;             /*!< The "data" subchunk contains the size of the data and the actual sound */
} __attribute__((packed)) pcm_wav_header_t;

typedef struct {
  wav_descriptor_chunk_t descriptor_chunk; /*!< Canonical WAVE format starts with the RIFF header */
  non_pcm_wav_fmt_chunk_t fmt_chunk;       /*!< The "fmt " subchunk describes the sound data's format */
  wav_data_chunk_t data_chunk;             /*!< The "data" subchunk contains the size of the data and the actual sound */
} __attribute__((packed)) non_pcm_wav_header_t;

#define WAVE_FORMAT_PCM        1  // PCM
#define WAVE_FORMAT_IEEE_FLOAT 3  // IEEE float
#define WAVE_FORMAT_ALAW       6  // 8-bit ITU-T G.711 A-law
#define WAVE_FORMAT_MULAW      7  // 8-bit ITU-T G.711 µ-law

#define PCM_WAV_HEADER_SIZE     44
#define NON_PCM_WAV_HEADER_SIZE 46

/**
 * @brief Default header for PCM format WAV files
 *
 */
#define PCM_WAV_HEADER_DEFAULT(wav_sample_size, wav_sample_bits, wav_sample_rate, wav_channel_num)                                              \
  {                                                                                                                                             \
    .descriptor_chunk =                                                                                                                         \
      {.chunk_id = {'R', 'I', 'F', 'F'}, .chunk_size = (wav_sample_size) + sizeof(pcm_wav_header_t) - 8, .chunk_format = {'W', 'A', 'V', 'E'}}, \
    .fmt_chunk =                                                                                                                                \
      {.subchunk_id = {'f', 'm', 't', ' '},                                                                                                     \
       .subchunk_size = 16,             /* 16 for PCM */                                                                                        \
       .audio_format = WAVE_FORMAT_PCM, /* 1 for PCM */                                                                                         \
       .num_of_channels = (uint16_t)(wav_channel_num),                                                                                          \
       .sample_rate = (wav_sample_rate),                                                                                                        \
       .byte_rate = (wav_sample_bits) * (wav_sample_rate) * (wav_channel_num) / 8,                                                              \
       .block_align = (uint16_t)((wav_sample_bits) * (wav_channel_num) / 8),                                                                    \
       .bits_per_sample = (uint16_t)(wav_sample_bits)},                                                                                         \
    .data_chunk = {                                                                                                                             \
      .subchunk_id = {'d', 'a', 't', 'a'},                                                                                                      \
      .subchunk_size = (wav_sample_size)                                                                                                        \
    }                                                                                                                                           \
  }
