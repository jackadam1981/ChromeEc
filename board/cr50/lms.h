#pragma once
#include "common.h"
#include "endian.h"

#define be32_t uint32_t
#define be16_t uint16_t

#define BE32(n)                                                                \
        (be32_t)((((be32_t)n >> 24) & 0xff) | (((be32_t)n >> 8) & 0xff00)      \
                 | (((be32_t)n << 8) & 0xff0000)                               \
                 | (((be32_t)n << 24) & 0xff000000))


#define BE16(n) ((((be16_t)n >> 8) & 0xff) | (((be16_t)n << 8) & 0xff00))

/*
 * This file has #define's that specify how this package operates, and
 * are designed to be tweaked by the user.
 *
 * These can be adjusted to be appropriate for what the application and
 * the operating environment needs
 */
#define HASH_SIZE_N32 SHA256_DIGEST_SIZE

/* Length of the largest hash we support */
#define MAX_HASH HASH_SIZE_N32
#define MAX_HASH_WORDS (HASH_SIZE_N32 / sizeof(uint32_t))
/* The I (Merkle tree identifier) value is 16 bytes long */
#define I_LEN_WORDS 4
#define I_LEN (I_LEN_WORDS * sizeof(uint32_t))
/* The maximum height of a Merkle tree */
#define MAX_MERKLE_HEIGHT 25
/* The mininum height of a Merkle tree.  Some of our update logic assumes */
/* this isn't too small */
#define MIN_MERKLE_HEIGHT 5
/* The minimum/maximum number of levels of Merkle trees within an HSS trees */
#define MIN_HSS_LEVELS 1 /* Minumum levels we allow */
#define MAX_HSS_LEVELS 8 /* Maximum levels we allow */

/* Here are some internal types used within the code.  They are listed more */
/* for documentation ("this is what this variable is expected to be") rather */
/* than to let the compiler do any sort of type checking */
/* This is an index into a Merkle tree */
/* Used for both the leaf index (0..N-1) and the node number (1..2*N-1), */
/* where N is the size 2**h of the tre */
#if MAX_MERKLE_HEIGHT > 31
/* We need to express more than 32 bits in this type */
typedef uint64_t merkle_index_t;
#error We need to extend the id we place within a hash to more than 4 bytes
#else
typedef uint32_t merkle_index_t;
#endif

typedef uint8_t digest_n32_t[HASH_SIZE_N32];
typedef uint32_t ilen_t[I_LEN_WORDS];

enum lms_descriptors
{
        D_PBLC = 0x8080,
        D_MESG = 0x8181,
        D_LEAF = 0x8282,
        D_INTR = 0x8383,
};
/* The initial message hashing */
#define MESG_I 0
#define MESG_Q 16
#define MESG_D 20 /* The fixed D_MESG value */
#define MESG_C 22
#define MESG_PREFIX_LEN(n) (MESG_C + (n)) /* Length not counting the actual */
                                          /* message being signed */
#define MESG_PREFIX_MAXLEN MESG_PREFIX_LEN(MAX_HASH)

/**
 * Defined LM parameter sets
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-208.pdf
 * https://www.rfc-editor.org/rfc/rfc8554.html
 * Additional paramters for SHA256/192 from:
 * https://tools.ietf.org/id/draft-fluhrer-lms-more-parm-sets-01.html
 * The encodings of the parameter sets are very fragile and should only be used
 * for testing purposes.
 */
enum lms_algorithm_type
{
        LMS_SHA256_M32_H5 = BE32(0x00000005),
        LMS_SHA256_M32_H10 = BE32(0x00000006),
        LMS_SHA256_M32_H15 = BE32(0x00000007),
        LMS_SHA256_M32_H20 = BE32(0x00000008),
        LMS_SHA256_M32_H25 = BE32(0x00000009),
        LMS_SHA256_M24_H5 = BE32(0xE0000001),
        LMS_SHA256_M24_H10 = BE32(0xE0000002),
        LMS_SHA256_M24_H15 = BE32(0xE0000003),
        LMS_SHA256_M24_H20 = BE32(0xE0000004),
        LMS_SHA256_M24_H25 = BE32(0xE0000005),
};

struct lms_params {
        enum lms_algorithm_type type;
        uint16_t h;
        uint16_t n;
};

/* LM-OTS registry */
enum lmots_algorithm_type
{
        LMOTS_SHA256_N32_W1 = BE32(0x00000001),
        LMOTS_SHA256_N32_W2 = BE32(0x00000002),
        LMOTS_SHA256_N32_W4 = BE32(0x00000003),
        LMOTS_SHA256_N32_W8 = BE32(0x00000004),
        LMOTS_SHA256_N24_W1 = BE32(0xE0000001),
        LMOTS_SHA256_N24_W2 = BE32(0xE0000002),
        LMOTS_SHA256_N24_W4 = BE32(0xE0000003),
        LMOTS_SHA256_N24_W8 = BE32(0xE0000004),
};

struct lmots_params {
        enum lmots_algorithm_type type;
        uint16_t logw;
        uint16_t n;
        uint16_t p;
        uint16_t ls;
        uint16_t max_digit; /* 2^w - 1 */
};


struct lms_signature {
        be32_t q;
        enum lmots_algorithm_type ots_alg_type;
        /* lmots signature + lms path */
        uint8_t y[]; /* first n bytes - message randomizer C */
                     /* then (p * n) bytes - hashes for symbols */
                     /* struct lmots_path_hdr follows */
}__packed __aligned(4);

struct lmots_path_hdr {
        be32_t parameter_set;
        uint8_t p[]; /* n-byte paths */
}__packed __aligned(4);

/* full signature length = 12 + 32 + (p*n) + 4 + n*height */
struct hss_signature {
        be32_t levels; /* should match public key */
        struct lms_signature lmots_sig;
}__packed __aligned(4);

/* We use a hash function based on the parameter set */

/**
 *  The LMS public key can be represented as the byte string
 *      u32str(type) || u32str(otstype) || I || T[1]
 *
 */
#define LM_PUB_PARM_SET 0     /* The parameter set (4 bytes) */
#define LM_PUB_OTS_PARM_SET 4 /* The OTS parameter set (4 bytes) */

/**
 * The LMS key pair identifier, I, shall be generated using an approved
 * random bit generator (see the SP 800-90 series of publications) where
 * the instantiation of the random bit generator supports at least
 * 128 bits of security strength.
 */
#define LM_PUB_I 8 /* Our nonce (I) value */
#define LM_PUB_K 24

struct lms_key {
        enum lms_algorithm_type lm_type;
        enum lmots_algorithm_type ots_alg_type;
        ilen_t I;
        uint32_t K[]; /* public key is a bare hash, length 24 or 32 bytes */
}__packed __aligned(4);







/* Hierarchical signature system (HSS) */
/* TODO: flatten it with lms_key? */
struct hss_public_key {
        be32_t levels; /* MIN_HSS_LEVELS .. MAX_HSS_LEVELS */
        struct lms_key lms_key;
}__packed __aligned(4);



/* Hashing Merkle tree leaf nodes */
#define LEAF_I 0
#define LEAF_R 16
#define LEAF_D 20
#define LEAF_PK 22
struct merkle_leaf {
        ilen_t I;    /* LEAF_I = 0 */
        be32_t r;    /* LEAF_R = 16 */
        be16_t d;    /* LEAF_D = 20 */
                     /* LEAF_PK = 22 */
        be16_t _pad; /* 2 byte padding to 32bit, not used */
}__packed __aligned(4);





/* Hashing Merkle tree internal nodes */
#define INTR_I 0
#define INTR_R 16
#define INTR_D 20
#define INTR_PK 22
#define INTR_MAX_LEN INTR_LEN(MAX_HASH)
struct merkle_internal {
        ilen_t I;    /* INTR_I = 0 */
        be32_t r;    /* INTR_R = 16 */
        be16_t d;    /* INTR_D = 20 */
                     /* INTR_PK = 22 */
        be16_t _pad; /* 2 byte padding for 32bit */
}__packed __aligned(4);






/**
 * The Winternitz iteration hashes
 * H(I || u32str(q) || u16str(i) || u8str(j) || tmp)
 */
#define ITER_I 0
#define ITER_Q 16
#define ITER_K 20 /* The RFC uses i here */
#define ITER_J 22
#define ITER_PREV 23 /* Hash from previous iteration; RFC uses tmp */
struct wots {
        ilen_t I;     /* ITER_I = 0 */
        be32_t q;     /* ITER_Q = 16 */
        be16_t k;     /* ITER_K = 20 */
        uint8_t j;    /* ITER_J = 22 */
        uint8_t _pad; /* to avoid warning */
} __packed __aligned(4);







/* Hashing the OTS public key */
#define PBLC_I 0
#define PBLC_Q 16
#define PBLC_D 20          /* The fixed D_PBLC value */
#define PBLC_PREFIX_LEN 22 /* Not counting the OTS public keys */
struct ots_public_key {
        ilen_t I; /* PBLC_I = 0 */
        be32_t q; /* PBLC_Q = 16 */
        be16_t d; /* PBLC_D = 20, The fixed D_PBLC value */
                  /* PBLC_PREFIX_LEN size */
}__packed __aligned(4);

const struct lmots_params *lm_ots_look_up_parameter_set(
        enum lmots_algorithm_type type);


struct Sha {
	uint32_t value[24 / 4];
}  __packed __aligned(4);

struct LeafSig {
        struct Sha hashes[26];
} __packed __aligned(4);

void xx_sha256_2(const void *input1, size_t size1, const void *input2,
		 size_t size2, uint32_t out[], size_t n);


void xx_sha256_3(const void *input1, size_t size1, const void *input2,
                        size_t size2, const void *input3,
                        size_t size3, uint32_t out[], size_t n);
void lm_ots_compute_pub_key(const struct lmots_params *lmots, const ilen_t I,
			    merkle_index_t q, const uint8_t private_data[],
			    struct Sha *pub_key_out);

void lm_ots_compute_sig(const struct lmots_params *lmots, const ilen_t I,
			merkle_index_t q, const uint8_t private_data[],
			const struct Sha* message_digest,
			struct LeafSig *leaf_sig_out);

void lm_ots_compute_pub_hash(
        const struct lmots_params *lmots, const ilen_t I, merkle_index_t q,
        struct Sha in_out_data[], const uint8_t num_hash[]);