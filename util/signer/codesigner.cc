
#include <iostream>
#include <fstream>
#include <sstream>
#include <ios>
#include <string>


#include <openssl/pem.h>



#include <signed_header.h>
#include <publickey.h>

using namespace std;
static uint8_t *mem; // this is where the file to sign is loaded
static size_t flat_size; // Size of the file to be signed (including 1Kb signature)


int LoadFlatFile(const char *name, size_t header_size)
{
  ifstream::pos_type fileSize;
  ifstream FlatFile (name, ios::in | ios::binary | ios::ate);

  if(!FlatFile.is_open()) {
    fprintf(stderr, "failed to open %s\n", name);
    return -1;
  }

  flat_size = FlatFile.tellg();

  mem = new uint8_t[flat_size];
  FlatFile.seekg(0, ios::beg);
  if(!FlatFile.read((char *)mem, flat_size)) {
    fprintf(stderr, "failed to read file %s\n", name);
    return -1;
  }
  FlatFile.close();

  // verify that there is enough room at the bottom
  for (size_t i = 0; i < header_size; i++)
    if (mem[i]) {
      fprintf(stderr, "nonzero value at offset %zd\n", i);
      return -1;
    }

  return 0;
}

int SaveSignedFile(const char *name)
{
  FILE* fp = fopen(name, "wb");

  if (!fp) {
    fprintf(stderr, "failed to open file '%s': %s\n", name, strerror(errno));
    return -1;
  }
  if (fwrite(mem, 1, flat_size, fp) != flat_size) {
    fprintf(stderr, "failed to write %zd bytes to '%s': %s\n",
            flat_size, name, strerror(errno));
    return -1;
  }
  fclose(fp);

  return 0;
}

static void sign(PublicKey& key, const SignedHeader* input_hdr) {
  BIGNUM* sig = NULL;
  SignedHeader* hdr = (SignedHeader*)(&mem[0]);
  int result;

  memcpy(hdr, input_hdr, sizeof(SignedHeader));

  result = key.sign(&hdr->tag, flat_size - offsetof(SignedHeader, tag), &sig);

  // Compute hash by hand as well.
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, &hdr->tag, flat_size - offsetof(SignedHeader, tag));
  SHA256_Final(hash, &sha256);
  fprintf(stderr, "hash:");
  for (size_t i = 0; i < sizeof(hash); ++i) {
    fprintf(stderr, "%02x", hash[i]);
  }
  fprintf(stderr, "\n");

  if (result != 1) {
    fprintf(stderr, "ossl_sign:%d\n", result);
  } else {
    hdr->image_size = flat_size;

    size_t nwords = key.nwords();
    key.toArray(hdr->signature, nwords, sig);
  }

  if (sig) BN_free(sig);
}

int main(int argc, char* argv[]) {
  bool doPrint = false;
  if (argc < 3) {
    fprintf(stderr, "Usage: %s pem-file [-p | hexfile]\n", argv[0]);
    exit(1);
  }
  const char* arg = argv[2];
  if (!strcmp(arg, "-p")) doPrint = true;

  PublicKey key(argv[1]);
  if (!key.ok()) return -1;

  if (doPrint) {
    // Print public key in C header format.
    key.print("ROMKEY");
    printf("#define ROMKEYEXP %u\n", key.public_exponent());
    printf("#define ROMKEY0INV 0x%08x\n", key.n0inv());
  } else {
    SignedHeader hdr;

    // Load input file
    if (LoadFlatFile(arg, sizeof(hdr)))
		  return -2;

    sign(key, &hdr);
    if (SaveSignedFile((std::string(arg) + std::string(".signed")).c_str()))
      return -2;
  }
  return 0;
}
