// This code is based on the base64 implementation from
// https://raw.githubusercontent.com/zhicheng/base64/master/base64.c
// and adapted from
// https://github.com/nkolban/esp32-snippets/tree/master/cloud/GCP/JWT

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/pk.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "jwt_manager.h"

enum
{
	BASE64_OK = 0,
	BASE64_INVALID
};

/* BASE 64 encode table */
static const char base64en[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_',
};

#define BASE64_PAD '='

#define BASE64DE_FIRST '+'
#define BASE64DE_LAST 'z'
/* ASCII order for BASE 64 decode, -1 in unused character */
static const signed char base64de[] = {
	/* '+', ',', '-', '.', '/', '0', '1', '2', */
	62,
	-1,
	-1,
	-1,
	63,
	52,
	53,
	54,

	/* '3', '4', '5', '6', '7', '8', '9', ':', */
	55,
	56,
	57,
	58,
	59,
	60,
	61,
	-1,

	/* ';', '<', '=', '>', '?', '@', 'A', 'B', */
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	0,
	1,

	/* 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', */
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,

	/* 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', */
	10,
	11,
	12,
	13,
	14,
	15,
	16,
	17,

	/* 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', */
	18,
	19,
	20,
	21,
	22,
	23,
	24,
	25,

	/* '[', '\', ']', '^', '_', '`', 'a', 'b', */
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	26,
	27,

	/* 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', */
	28,
	29,
	30,
	31,
	32,
	33,
	34,
	35,

	/* 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', */
	36,
	37,
	38,
	39,
	40,
	41,
	42,
	43,

	/* 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', */
	44,
	45,
	46,
	47,
	48,
	49,
	50,
	51,
};

int base64url_encode(const unsigned char* in, unsigned int inlen, char* out)
{
	unsigned int i, j;

	for (i = j = 0; i < inlen; i++) {
		int s = i % 3; /* from 6/gcd(6, 8) */

		switch (s) {
			case 0:
				out[j++] = base64en[(in[i] >> 2) & 0x3F];
				continue;
			case 1:
				out[j++] = base64en[((in[i - 1] & 0x3) << 4) + ((in[i] >> 4) & 0xF)];
				continue;
			case 2:
				out[j++] = base64en[((in[i - 1] & 0xF) << 2) + ((in[i] >> 6) & 0x3)];
				out[j++] = base64en[in[i] & 0x3F];
		}
	}

	/* move back */
	i -= 1;

	/* check the last and add padding */

	if ((i % 3) == 0) {
		out[j++] = base64en[(in[i] & 0x3) << 4];
		// out[j++] = BASE64_PAD;
		// out[j++] = BASE64_PAD;
	} else if ((i % 3) == 1) {
		out[j++] = base64en[(in[i] & 0xF) << 2];
		// out[j++] = BASE64_PAD;
	}

	out[j++] = 0;

	return BASE64_OK;
}

int base64url_decode(const char* in, unsigned int inlen, unsigned char* out)
{
	unsigned int i, j;

	for (i = j = 0; i < inlen; i++) {
		int c;
		int s = i % 4; /* from 8/gcd(6, 8) */

		if (in[i] == '=')
			return BASE64_OK;

		if (in[i] < BASE64DE_FIRST || in[i] > BASE64DE_LAST ||
			(c = base64de[in[i] - BASE64DE_FIRST]) == -1)
			return BASE64_INVALID;

		switch (s) {
			case 0:
				out[j] = ((unsigned int)c << 2) & 0xFF;
				continue;
			case 1:
				out[j++] += ((unsigned int)c >> 4) & 0x3;

				/* if not last char with padding */
				if (i < (inlen - 3) || in[inlen - 2] != '=')
					out[j] = ((unsigned int)c & 0xF) << 4;
				continue;
			case 2:
				out[j++] += ((unsigned int)c >> 2) & 0xF;

				/* if not last char with padding */
				if (i < (inlen - 2) || in[inlen - 1] != '=')
					out[j] = ((unsigned int)c & 0x3) << 6;
				continue;
			case 3:
				out[j++] += (unsigned char)c;
		}
	}

	return BASE64_OK;
}

/**
 * Return a string representation of an mbedtls error code
 */
static char* mbedtlsError(int errnum)
{
	static char buffer[200];
	mbedtls_strerror(errnum, buffer, sizeof(buffer));
	return buffer;
} // mbedtlsError

/**
 * Create a JWT token for GCP.
 * For full details, perform a Google search on JWT.  However, in summary, we build two strings. One
 * that represents the header and one that represents the payload.  Both are JSON and are as
 * described in the GCP and JWT documentation.  Next we base64url encode both strings.  Note that is
 * distinct from normal/simple base64 encoding.  Once we have a string for the base64url encoding of
 * both header and payload, we concatenate both strings together separated by a ".".   This
 * resulting string is then signed using RSASSA which basically produces an SHA256 message digest
 * that is then signed.  The resulting binary is then itself converted into base64url and
 * concatenated with the previously built base64url combined header and payload and that is our
 * resulting JWT token.
 * @param email The email address of the service account.
 * @param privateKey The PEM or DER of the private key.
 * @param privateKeySize The size in bytes of the private key.
 * @returns A JWT token for transmission to GCP.
 */
char* createGCPJWT(const char* email, uint8_t* privateKey, size_t privateKeySize)
{
	char base64Header[40];
	const char header[] = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
	base64url_encode((unsigned char*)header, // Data to encode.
					 strlen(header),		 // Length of data to encode.
					 base64Header);			 // Base64 encoded data.

	time_t iat = time(NULL); // Set the time now.
	time_t exp = iat + 60;	 // Set the expiry time.

	char payload[300];
	sprintf(payload,
			"{\"iat\":%lld,\"exp\":%lld,\"aud\":\"https://oauth2.googleapis.com/token\","
			"\"iss\":\"%s\",\"scope\":\"https://www.googleapis.com/auth/calendar.readonly\"}",
			iat,
			exp,
			email);

	char base64Payload[300];
	base64url_encode((unsigned char*)payload, // Data to encode.
					 strlen(payload),		  // Length of data to encode.
					 base64Payload);		  // Base64 encoded data.

	uint8_t headerAndPayload[400];
	sprintf((char*)headerAndPayload, "%s.%s", base64Header, base64Payload);

	// At this point we have created the header and payload parts, converted both to base64 and
	// concatenated them together as a single string.  Now we need to sign them using RSASSA

	mbedtls_pk_context pk_context;
	mbedtls_pk_init(&pk_context);

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);

	const char* pers = "EPD Screen";

	mbedtls_ctr_drbg_seed(
	  &ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));

	int rc = mbedtls_pk_parse_key(
	  &pk_context, privateKey, privateKeySize, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
	if (rc != 0) {
		printf("Failed to mbedtls_pk_parse_key: %d (-0x%x): %s\n", rc, -rc, mbedtlsError(rc));
		return NULL;
	}

	uint8_t oBuf[1000];

	uint8_t digest[32];
	rc = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
					headerAndPayload,
					strlen((char*)headerAndPayload),
					digest);
	if (rc != 0) {
		printf("Failed to mbedtls_md: %d (-0x%x): %s\n", rc, -rc, mbedtlsError(rc));
		return NULL;
	}

	size_t retSize;
	rc = mbedtls_pk_sign(&pk_context,
						 MBEDTLS_MD_SHA256,
						 digest,
						 sizeof(digest),
						 oBuf,
						 sizeof(oBuf),
						 &retSize,
						 mbedtls_ctr_drbg_random,
						 &ctr_drbg);
	if (rc != 0) {
		printf("Failed to mbedtls_pk_sign: %d (-0x%x): %s\n", rc, -rc, mbedtlsError(rc));
		return NULL;
	}

	char base64Signature[400];
	base64url_encode((unsigned char*)oBuf, retSize, base64Signature);

	char* retData =
	  (char*)malloc(strlen((char*)headerAndPayload) + 1 + strlen((char*)base64Signature) + 1);

	sprintf(retData, "%s.%s", headerAndPayload, base64Signature);

	mbedtls_pk_free(&pk_context);
	return retData;
}