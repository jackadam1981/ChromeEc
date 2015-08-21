#include <assert.h>

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/sha.h>

#include <string>

extern "C" {
#include <mpsse.h>
}

bool FLAGS_corrupt = false;  // For testing.
bool FLAGS_verbose = true;

/**
 * Little wrapper around essentially SHA256 output
 * With convenience comparison and assignment operators.
 */
typedef struct Ack {
  uint8_t hash[32];

  Ack(uint8_t v) {
    memset(this->hash, v, sizeof this->hash);
  }

  bool operator==(const struct Ack& other) {
    return !memcmp(this->hash, other.hash, sizeof this->hash);
  }

  bool operator!=(const struct Ack& other) {
    return memcmp(this->hash, other.hash, sizeof this->hash);
  }

  void Print(const char* tag) {
    printf("%s: ", tag);
    for (size_t i = 0; i < sizeof this->hash; ++i) {
      printf("%02x", this->hash[i]);
    }
  printf("\n");
  }
} Ack;
static_assert(sizeof(Ack) == 32, "Ack should be 32 bytes");

/*
 * We use a 1024 byte struct for communication, which matches the SPI rx fifo
 * size for easy stop/go synchronisation.
 */
#define SIZE (1024-32-4)
#define FRAMENO(k) ((k)&0xffffff)
#define FLAG_FINAL_FRAME 0x80000000
typedef struct Frame {
  uint8_t hash[32];
  uint32_t no;
  uint8_t data[SIZE];

  void Transfer(struct mpsse_context* spi, Ack* reply) {
    usleep(10);  // <-- important? seems to corrupt otherwise.

    if (FLAGS_verbose) {
      printf("%d.", FRAMENO(this->no));
      fflush(stdout);
    }

    // Fudge for testing.
    bool corrupt = false;
    if (FLAGS_corrupt) {
      corrupt = (rand() % 3 == 0);
      if (corrupt) {
        this->hash[0] ^= 1;
      }
    }

    ::Start(spi);
    char* ack = ::Transfer(spi, reinterpret_cast<char*>(this), sizeof *this);
    ::Stop(spi);

    // Restore original.
    if (corrupt) {
      this->hash[0] ^= 1;
    }

    assert(ack != NULL);

    memcpy(reply->hash, ack, 32);
    free(ack);
  }

  void Populate(int k, const uint8_t* code) {
    this->no = k;
    memcpy(this->data, code + FRAMENO(k) * SIZE, sizeof this->data);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, &this->no, sizeof this->no);
    SHA256_Update(&sha256, this->data, sizeof this->data);
    SHA256_Final(this->hash, &sha256);
  }

  void ComputeAck(Ack* current_ack) {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, this->hash, sizeof this->hash);
    SHA256_Update(&sha256, &this->no, sizeof this->no);
    SHA256_Update(&sha256, this->data, sizeof this->data);
    SHA256_Final(current_ack->hash, &sha256);

    // touch up zeros into ones.
    for (int i = 0; i < 32; ++i) {
      current_ack->hash[i] |= !current_ack->hash[i];
    }
  }
} Frame;
static_assert(sizeof(Frame) == 1024, "Frame size should be 1024");


int main(int argc, char* argv[]) {
  struct mpsse_context *spi = NULL;

  // TODO: real flag parsing.
  FLAGS_corrupt = (argc > 2);

  srand(time(NULL));

  IntelHex ihex(argv[1]);
  if (!ihex.ok()) {
    return -1;
  }

  // Switch to SPI mode and transmit code.
  if ((spi = MPSSE(SPI0, 1000000/**/, MSB, NULL)) != NULL && spi->open) {
    printf("%.6s initialized at %dHz (SPI mode 0)\n",
           GetDescription(spi), GetClock(spi));

    // Pull GPIO0 low to signal we want to bootstap.
    //
    // Note only right after reset does bootrom pay attention to this,
    // so one is best to hit reset on the target now.
    ::PinLow(spi, GPIOL0);

    ::PinLow(spi, GPIOL1);
    usleep(100000);
    ::PinHigh(spi, GPIOL1);

    Frame f, prev_f;

    Ack expected_ack(0);
    Ack no_ack(0);
    Ack current_ack(0xff);
    Ack received_ack(0);

    int maxframe = (ihex.size() + SIZE - 1) / SIZE;

    for (int frameno = 0; FRAMENO(frameno) < maxframe; ++frameno) {
      prev_f = f;
      expected_ack = current_ack;

      if (frameno == maxframe - 1) {
        frameno |= FLAG_FINAL_FRAME;
      }

      f.Populate(frameno, ihex.code());
      f.ComputeAck(&current_ack);

      if (frameno == 0) {
        prev_f = f;
        expected_ack = current_ack;
      }

      do {
        f.Transfer(spi, &received_ack);

        if (received_ack == no_ack) {
          // No ack; just resend frame and keep polling.
          continue;
        }

        if (received_ack != expected_ack) {
          // Got ack, but not right one.
          received_ack.Print("\nreceived_ack");
          expected_ack.Print("expected_ack");

          // Resend previous frame until ack'ed.
          do {
            prev_f.Transfer(spi, &received_ack);
          } while (received_ack != expected_ack);
        }
      } while (received_ack != expected_ack);
    }

    // Try to get ack for final frame.
    // (but note target might have already re-initialized SPI interface)
    do {
      f.Transfer(spi, &received_ack);
    } while (received_ack != current_ack);

    printf("done\n");

    // Release GPIO0, done booting.
    ::PinHigh(spi, GPIOL0);
  }

  if (spi) Close(spi);

  return 0;
}
