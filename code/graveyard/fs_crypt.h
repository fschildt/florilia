// Note: 
// - The implementation is not done yet at all.
// - used rsa algorithm: https://www.di-mgt.com.au/rsa_alg.html

// Todo: bulletproof security
// - Ensure that that rsa factors (p,q) are not close to each other
//   because it's easy to attack, see https://youtube.com/watch?v=-ShwJqAalOk
//
// - Do the best you can for random number generation
//   - for linux: "Wait" for high entropy for better random numbers
//     - /proc/sys/kernel/random/poolsize
//     - /proc/sys/kernel/random/entropy_avail



/*********************
 *     INTERFACE     *
 *********************/

#ifndef FS_CRYPT_INTERFACE
#define FS_CRYPT_INTERFACE

#include <stdint.h>

#ifndef FSCRYPT_DEF
#define FSCRYPT_DEF static
#endif


//
// random number generation
//
#define FSCRYPT_GEN_RANDOM(name) int name(uint8_t* buff, uint32_t size)
typedef FSCRYPT_GEN_RANDOM(fscrypt_gen_random);


// 
// RSA
//
typedef struct {
    uint32_t prime_size;
    uint32_t e;
    uint8_t  n[2048]; // (e,n) is public key
    uint8_t  d[2048]; // (d,n) is private key
} fscrypt_rsa_2048;

typedef struct {
    uint32_t prime_size;
    uint32_t e;
    uint8_t  n[4096]; // (e,n) is public key
    uint8_t  d[4096]; // (d,n) is private key
} fscrypt_rsa_4096;

typedef union {
    fscrypt_rsa_2048 rsa_2048;
    fscrypt_rsa_4096 rsa_4096;
} fscrypt_rsa;
// fscrypt_rsa is an allocated fscrypt_rsa_xxxx structure, that's casted to fscrypt_rsa* internally.
// prime_size should be 2048 or 4096 and e should be 3, 5, 17, 257, 65537.

FSCRYPT_DEF int fscrypt_gen_rsa(fscrypt_rsa *rsa, uint32_t prime_size, uint32_t e, fscrypt_gen_random *gen_random);
FSCRYPT_DEF void fscrypt_rsa_encrypt(fscrypt_rsa *rsa, void *clear, void *cipher);
FSCRYPT_DEF void fscrypt_rsa_decrypt(fscrypt_rsa *rsa, void *cipher, void *clear);

#endif // FSCRYPT_INTERFACE




/**************************
 *     IMPLEMENTATION     *
 **************************/

#ifdef FS_CRYPT_IMPLEMENTATION

#include <math.h>

static int fscrypt__unsigned_add(uint8_t *result, uint8_t *a, uint8_t *b, uint32_t size)
{
    int it = size - 1;
    uint16_t carry = 0;
    while (it >= 0)
    {
        uint16_t res = (uint16_t)a[it] + (uint16_t)b[it] + carry;
        if (res > 0xff)
        {
            res -= 0xff;
            carry = 1;
        }

        result[it] = res;
        it--;
    }

    int success = carry == 0 ? 1 : 0;
    return success;
}

static int fscrypt__unsigned_sub(uint8_t *result, uint8_t *a, uint8_t *b, uint32_t size)
{
    int32_t it = size-1;
    uint16_t carry = 0;
    while (it >= 0)
    {
        uint8_t subtrahend;
        if (carry)
        {
            if (b[it] == 0xff)
            {
                subtrahend = 0;
                carry++;
            }
            else
                subtrahend = b[it] + 1;

            carry--;
        }
        else
            subtrahend = b[it];

        uint16_t minuend = a[it];
        if (minuend < subtrahend)
        {
            minuend += 0xff;
            carry++;
        }

        result[it] = (uint8_t)(minuend - subtrahend);

        it--;
    }

    int success = carry == 0;
    return success;
}

// Todo: @OPTIMIZE, only process the necessary bytes, not the whole buffer
static int fscrypt__unsigned_dec(uint8_t *buff, uint32_t size)
{
    uint8_t one[size];
    memset(one, 0, size);
    one[size-1] = 1;

    int success = fscrypt__unsigned_sub(buff, buff, one, size);
    return success;
}

// Todo: @OPTIMIZE, only process the necessary bytes, not the whole buffer
static int fscrypt__unsigned_inc(uint8_t *buff, uint32_t size)
{
    uint8_t one[size];
    memset(one, 0, size);
    one[size-1] = 1;

    int success = fscrypt__unsigned_add(buff, buff, one, size);
    return success;
}

static void fscrypt__gcd(uint8_t *a, uint8_t *b, uint32_t size)
{
}

static uint32_t fscrypt__mod(uint8_t *buff, uint8_t size, uint32_t mod)
{
    return 0;
}

static uint32_t fscrypt__mod2(uint8_t *a, uint8_t *b, uint8_t* modulus, uint32_t size)
{
    return 0;
}

// Algorithm used:
// https://www.geeksforgeeks.org/prime-numbers/
// - (the more effective approach)
static int fscrypt__is_prime(uint8_t *buff, uint32_t size)
{
    uint8_t zero[size];
    uint8_t one[size];
    uint8_t two[size];
    uint8_t three[size];



    // Check if n=1 or n=0
    memset(cmp, 0, size);
    if (memcmp(cmp, buff, size) == 0)
        return false;

    cmp[size-1] = 1;
    if (memcmp(cmp, buff, size) == 0)
        return false;


    // Check if n=2 or n=3
    cmp[size-1] = 2;
    if (memcmp(cmp, buff, size) == 0)
        return true;

    cmp[size-1] = 3;
    if (memcmp(cmp, buff, size) == 0)
        return true;


    // Todo: Check whether n is divisible by 2 or 3


    return true;
}

static int fscrypt__gen_prime(uint8_t *buff, uint32_t size, fscrypt_gen_random *gen_random)
{
    if (!gen_random(buff, size))
        return 0;

    while (!fscrypt__is_prime(buff, size))
        fscrypt__unsigned_inc(buff, size);

    return 1;
}

static int fscrypt__gen_pq(uint8_t *p, uint8_t *q, uint32_t size, uint32_t e, fscrypt_gen_random *gen_random)
{
    while (1)
    {
        uint32_t remainder;
        if (!fscrypt__gen_prime(p, size, gen_random))
            return 0;

        if (fscrypt__mod(p, size, e) == 1)
            break;
    }

    while (1)
    {
        if (!fscrypt__gen_prime(q, size, gen_random)) 
            return 0;

        if (fscrypt__mod(q, size, e) == 1)
            break;
    }

    return 1;
}

FSCRYPT_DEF void fscrypt_rsa_encrypt(fscrypt_rsa *rsa, void *clear, void *cipher)
{
}

FSCRYPT_DEF void fscrypt_rsa_decrypt(fscrypt_rsa *rsa, void *cipher, void *clear)
{
}

FSCRYPT_DEF int fscrypt_gen_rsa(fscrypt_rsa *rsa, uint32_t prime_size, uint32_t e, fscrypt_gen_random *gen_random)
{
    uint8_t  *n;
    uint8_t  *d;
    if (prime_size == 2048)
    {
        fscrypt_rsa_2048 *rsa_2048 = (fscrypt_rsa_2048*)rsa;
        n = rsa_2048->n;
        d = rsa_2048->d;

        rsa_2048->e = e;
        rsa_2048->prime_size = prime_size;
    }
    else if (prime_size == 4096)
    {
        fscrypt_rsa_4096 *rsa_4096 = (fscrypt_rsa_4096*)rsa;
        n = rsa_4096->n;
        d = rsa_4096->d;

        rsa_4096->e = e;
        rsa_4096->prime_size = prime_size;
    }
    else
        return 0;


    uint8_t p[prime_size];
    uint8_t q[prime_size];
    uint8_t phi[prime_size*2];


    if (!fscrypt__gen_pq(p, q, prime_size, e, gen_random))
        return 0;

    // n = p * q
//    fscrypt__unsigned_mul(n, p, q, prime_size, prime_size);

    // phi = (p-1)(q-1)
    fscrypt__unsigned_dec(p, prime_size);
    fscrypt__unsigned_dec(q, prime_size);
//    fscrypt__unsigned_mul(phi, p, q, prime_size, prime_size);

#if 0
    // solve e*d % phi for d
    // apparently this is equivalent to solving e*x + phi*y for x
    int is_negative;
    fscrypt__extended_euclid(d, e, phi, &is_negative);
    if (is_negative)
    {
        unsign(d);
        unsigned_sub(d, phi, d);
    }
#endif

    return 1;
}

#endif // FSCRYPT_IMPLEMENTATION
