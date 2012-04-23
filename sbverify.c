/*
 * Copyright (C) 2012 Jeremy Kerr <jeremy.kerr@canonical.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include "image.h"

#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pkcs7.h>


int main(int argc, char **argv)
{
	struct cert_table_header *header;
	struct image *image;
	uint8_t *idcbuf, tmp;
	const uint8_t *buf;
	int idclen, rc;
	BIO *idcbio;
	PKCS7 *p7;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <boot-image>\n", argv[0]);
		return EXIT_FAILURE;
	}

	image = image_load(argv[1]);
	image_pecoff_parse(image);

	header = image->buf + image->data_dir_sigtable->addr;

	ERR_load_crypto_strings();
	OpenSSL_add_all_digests();
	buf = (void *)(header + 1);
	p7 = d2i_PKCS7(NULL, &buf, header->size);

	idcbuf = p7->d.sign->contents->d.other->value.asn1_string->data;

	/* we don't include the type and length data in the hash */
	tmp = idcbuf[1];
	if (!(tmp & 0x80)) {
		idclen = tmp & 0x7f;
		idcbuf += 2;
	} else if ((tmp & 0x82) == 0x82) {
		idclen = (idcbuf[2] << 8) +
			 idcbuf[3];
		idcbuf += 4;
	}

	idcbio = BIO_new(BIO_s_mem());
	BIO_write(idcbio, idcbuf, idclen);

	rc = PKCS7_verify(p7, NULL, NULL, idcbio, NULL,
			PKCS7_BINARY | PKCS7_NOVERIFY);

	if (!rc) {
		printf("PKCS7 verification failed\n");
		ERR_print_errors_fp(stderr);
	} else {
		printf("Signature verification OK\n");
	}

	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
}
