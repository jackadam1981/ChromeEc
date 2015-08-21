#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <common/intelhex.h>
#include <common/publickey.h>
#include <common/signed_header.h>
#include <rapidjson/document.h>

#include <iostream>
#include <fstream>
#include <sstream>

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
    // Load hexFile, sign it and output to stdout.
    IntelHex ih(arg, 0x400);
    if (!ih.ok()) return -2;

    SignedHeader hdr;

    //TODO: fill in hdr.tag, hdr.fusemap

    ih.sign(key, &hdr);
    ih.print();
  }

  // JSON parse test code.
  if (argc > 3) {
    std::ifstream ifs(argv[3]);
    if (ifs) {
      std::string s;
      while (ifs) {
        std::string line;
        std::getline(ifs, line);
        size_t nonspace = line.find_first_not_of(" \t");
        if (nonspace != std::string::npos &&
            line.find("//", nonspace) == nonspace) {
          // Drop comment lines, which may start with whitespace.
          continue;
        }
        s.append(line);
      }

      rapidjson::Document d;
      if (d.Parse(s.c_str()).HasParseError()) {
        fprintf(stderr, "JSON %s[%lu]: parse error\n",
                argv[3], d.GetErrorOffset());
      } else {
        const rapidjson::Document::ValueType& fuses = d["fuses"];
        const rapidjson::Document::ValueType& conditions = d["conditions"];
        for (auto it = conditions.MemberBegin();
             it != conditions.MemberEnd(); ++it) {
          const char* name = it->name.GetString();
          if (!fuses.HasMember(name)) {
            fprintf(stderr, "Fuse '%s' undefined\n", name);
          } else
          printf("%s:%u@%u\n",
                 name,
                 it->value.GetBool(),
                 fuses[name].GetInt());
        }
      }
    }
  }

  return 0;
}
