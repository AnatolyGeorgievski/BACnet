/* RFC 8163                IPv6 over MS/TP (6LoBAC)                May 2017

[ANSI/ASHRAE Standard 135-2016] ANNEX T - COBS (CONSISTENT OVERHEAD BYTE STUFFING) FUNCTIONS (INFORMATIVE)
*/
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/*! \brief копирование строк не более заданной длины
	\return длина остатка строки
 */
#pragma GCC optimize ("O2")
/*! \brief Кодирование данных методом COBS

    количество дананных при кодировании немного увеличиваться, на пакет размером 1500 байт,
    на каждые 254 байта идет увеличение на один байт, в итоге 6 байт. можно писать поверх исходных данных
    dst = src-6;
    кодирование производится по маске 0x55, с целью исключить преамблу. Однако преамбла может быть в шапке.

    \param dst writes one or more COBS code blocks at 'dst', removing any
    \param src
    \param length - длина некодированных данных
	\return the length of the encoded data.
 */
size_t cobs_encode(uint8_t * dst, uint8_t * src, size_t length)
{
	uint8_t *ref = dst;
	uint8_t code;
	size_t len;
	do {
        len = length>254? 254: length;
        uint8_t *s = src;
		uint8_t *d = dst++;
		do {
			uint8_t ch = *src++;
			if (ch==0) break;
			*dst++=ch ^ 0x55;

		} while(--len);

		code = (src-s);
		d[0] = (len?code:code+1) ^ 0x55;
	} while (length-=code);
	if (len==1) *dst++= 0x54;
	return dst-ref;
}
/*! \brief Декодирование COBS
    \param dst буфер данных для декодирования, можно писать поверх исходных данных, при декодировании объем данных уменьшается
    \param src буфер кодированных данных
    \param length длина кодированных данных
    \return the length of the encoded data or zero if error.
 */
size_t cobs_decode(uint8_t * dst, uint8_t * src, size_t length)
{
	uint8_t *d = dst;
	uint8_t code;
	do {
		uint8_t len = *src++ ^ 0x55;
		code = len;
		while(--len) {
			*dst++ = *src++ ^ 0x55;
		}
		if ((length-=code)==0) break;
		if(code!=255)*dst++=0;
	} while(1);
	// фантомный нолик
	return dst-d;
}
static const uint32_t crc32k_table[] =  {
0x00000000, 0x83CF0F3C, 0xD1FDAE25, 0x5232A119,
0x7598EC17, 0xF657E32B, 0xA4654232, 0x27AA4D0E,
0xEB31D82E, 0x68FED712, 0x3ACC760B, 0xB9037937,
0x9EA93439, 0x1D663B05, 0x4F549A1C, 0xCC9B9520,
};

static uint32_t bacnet_crc32k_update(uint32_t crc, uint8_t val){
	crc = (crc>>4) ^ crc32k_table[(crc ^ (val   )) & 0xF];
	crc = (crc>>4) ^ crc32k_table[(crc ^ (val>>4)) & 0xF];
	return crc;
}

//extern uint32_t bacnet_crc32k_update(uint32_t crc, uint8_t val);
int cobs_crc32k_check(uint8_t * data, size_t length)
{
	uint32_t crc32k = ~0UL;
	int i;
	for (i=0; i<length; i++)
		crc32k = bacnet_crc32k_update(crc32k, data[i]);
	return (data[length]==0x50) && (*(uint32_t*)(data+length+1) == ((~crc32k)^(0x55555555UL)));
}
/*! \brief Добавить контрольную сумму в конец кадра
	расчет контрольной суммы кадра производится методом CRC32 (Koopman)
	\see ...

 */
uint32_t bacnet_crc32k(uint8_t * data, size_t length)
{
	uint32_t crc32k = ~0UL;
	int i;
	for (i=0; i<length; i++)
		crc32k = bacnet_crc32k_update(crc32k, data[i]);
    return crc32k;
}
#if 0
size_t cobs_crc32k(uint8_t * data, size_t length)
{
	uint32_t crc32k = ~0UL;
	int i;
	for (i=0; i<length; i++)
		crc32k = bacnet_crc32k_update(crc32k, data[i]);
	data[length] = 0x50;
	*(uint32_t *)(data+length+1) = (~crc32k)^(0x55555555UL);// это прокатывает для Eittle-Endian
	return length+5;
}
#endif // 0
#ifdef TEST_COBS
size_t cobs_encode1 (uint8_t *to, const uint8_t *from, size_t length, uint8_t mask)
{
    size_t code_index = 0;
    size_t read_index = 0;
    size_t write_index = 1;
    uint8_t code = 1;
    uint8_t data, last_code=0;

    while (read_index < length)
    {
        data = from[read_index++];
        /* In the case of encountering a non-zero octet in the data,
         * simply copy input to output and increment the code octet.
         */
        if (data != 0)
        {
            to[write_index++] = data ^ mask;
            code++;
            if (code != 255)
                continue;
        }
        /* In the case of encountering a zero in the data or having
         * copied the maximum number (254) of non-zero octets, store
         * the code octet and reset the encoder state variables.
         */
        last_code = code;
        to[code_index] = code ^ mask;
        code_index = write_index++;
        code = 1;
    }
    /* If the last chunk contains exactly 254 non-zero octets, then
     * this exception is handled above (and the returned length must
     * be adjusted). Otherwise, encode the last chunk normally, as if
     * a "phantom zero" is appended to the data.
     */
    if ((last_code == 255) && (code == 1))
        write_index--;
    else
        to[code_index] = code ^ mask;
    return write_index;
}
/*! Decodes 'length' octets of data located at 'from' and
 * writes the original client data at 'to', restoring any
 * 'mask' octets that may present in the encoded data.
 * Returns the length of the encoded data or zero if error.
 */
size_t cobs_decode1 (uint8_t *to, const uint8_t *from, size_t length, uint8_t mask)
{
    size_t read_index = 0;
    size_t write_index = 0;
    uint8_t code, last_code;

    while (read_index < length)
    {
        code = from[read_index] ^ mask;
        last_code = code;
        /* Sanity check the encoding to prevent the while() loop below
         * from overrunning the output buffer.
         */
        if (read_index + code > length)
            return 0;

        read_index++;
        while (--code > 0)
            to[write_index++] = from[read_index++] ^ mask;
        /* Restore the implicit zero at the end of each decoded block
         * except when it contains exactly 254 non-zero octets or the
         * end of data has been reached.
         */
        if ((last_code != 255) && (read_index < length))
            to[write_index++] = 0;
    }
    return write_index;
}
/*
As an example, the frame encoding of the null-terminated C string "Hello World\n" is shown below. Before encoding, the
client data is:
0000: 48 65 6C 6C 6F 20 57 6F 72 6C 64 0A 00 		"Hello World.."
After COBS encoding (shown here for clarity), the output stream is:
0000: 0D 48 65 6C 6C 6F 20 57 6F 72 6C 64 0A 01 	".Hello World.."
Each octet in the COBS-encoded output stream is XOR'ed with X'55':
0000: 58 1D 30 39 39 3A 75 02 3A 27 39 31 5F 54 	"X.099:u.:'91_T"
The length of the resulting Encoded Data field is 14 octets. After adding the constant five octet length of the Encoded CRC-
32K field and subtracting two octets for compatibility with legacy MS/TP devices, the resulting Length field is 17 octets. At
this point data transmission may begin and each octet in the Encoded Data field is accumulated in the CRC-32K before it is sent.
The resulting CRC-32K value (shown here for clarity) is:
000E: B3 8F 28 CA
After taking the ones' complement and arranging in LSB order (see Clause G.3), the value becomes:
000E: 35 D7 70 4C
After COBS-encoding and XOR'ing each octet in the output stream with X'55', the Encoded CRC-32K field is ready for
transmission:
000E: 50 60 82 25 19
*/
#define CRC32K_INITIAL_VALUE (0xFFFFFFFF)
#define CRC32K_RESIDUE (0x0843323B)
#define MSTP_PREAMBLE_X55 (0x55)


#define ADJ_FOR_ENC_CRC (5) /* Set to 3 if passing MS/TP Length field */
#define SIZEOF_ENC_CRC (5)

/*
* Encodes ’length’ octets of client data located at 'from' and writes
* the COBS-encoded Encoded Data and Encoded CRC-32K fields at 'to'.
* Returns the combined length of these encoded fields.
*/
size_t
frame_encode (uint8_t *to, const uint8_t *from, size_t length)
{
    size_t cobs_data_len, cobs_crc_len;
    uint32_t crc32K;
    int i;
    /*
    * Prepare the Encoded Data field for transmission.
    */
    cobs_data_len = cobs_encode1(to, from, length, MSTP_PREAMBLE_X55);
    /*
    * Calculate CRC-32K over the Encoded Data field.
    * NOTE: May be done as each octet is transmitted to reduce latency.
    */
    crc32K = CRC32K_INITIAL_VALUE;
    for (i = 0; i < cobs_data_len; i++)
    {
        crc32K = bacnet_crc32k_update(crc32K, to[i]); /* See Clause G.3.1 */
    }
    /*
    * Prepare the Encoded CRC-32K field for transmission.
    * NOTE: Assumes a little-endian CPU (otherwise order the
    * octets least-significant first before encoding).
    */
    crc32K = ~crc32K;
    cobs_crc_len = cobs_encode1((uint8_t *)(to + cobs_data_len),
                               (const uint8_t *)&crc32K, sizeof(uint32_t),
                               MSTP_PREAMBLE_X55);
    /*
    * Return the combined length of the Encoded Data and Encoded CRC-32K
    * fields. NOTE: Subtract two before use as the MS/TP frame Length field.
    */
    return cobs_data_len + cobs_crc_len;
}
/*
* Decodes Encoded Data and Encoded CRC-32K fields at 'from' and
* writes the decoded client data at 'to'. Assumes 'length' contains
* the actual combined length of these fields in octets (that is, the
* MS/TP header Length field plus two).
* Returns length of decoded Data in octets or zero if error.
* NOTE: Safe to call with ’output’ <= ’input’ (decodes in place).
*/
size_t
frame_decode (uint8_t *to, const uint8_t *from, size_t length)
{
    size_t data_len, crc_len;
    uint32_t crc32K;
    int i;
    /*
    * Calculate the CRC32K over the Encoded Data octets before decoding.
    * NOTE: Adjust 'length' by removing size of Encoded CRC-32K field.
    */
    data_len = length - ADJ_FOR_ENC_CRC;
    crc32K = CRC32K_INITIAL_VALUE;
    for (i = 0; i < data_len; i++)
    {
        crc32K = bacnet_crc32k_update(crc32K, from[i]); /* See Clause G.3.1 */
    }
    data_len = cobs_decode1(to, from, data_len, MSTP_PREAMBLE_X55);
    /*
    * Decode the Encoded CRC-32K field and append to data.
    */
    crc_len = cobs_decode1((uint8_t *)(to + data_len),
                          (uint8_t *)(from + length - ADJ_FOR_ENC_CRC),
                          SIZEOF_ENC_CRC,
                          MSTP_PREAMBLE_X55);
    /*
    * Sanity check length of decoded CRC32K.
    */
    if (crc_len != sizeof(uint32_t))
    {
        return 0;
    }
    /*
    * Verify CRC32K of incoming frame.
    */
    for (i = 0; i < crc_len; i++)
    {
        crc32K = bacnet_crc32k_update(crc32K, (to + data_len)[i]);
    }
    if (crc32K == CRC32K_RESIDUE)
    {
        return data_len;
    }
    else
    {
        return 0;
    }
}
/* Accumulate "dataValue" into the CRC in "crc32kValue".
* Return value is updated CRC.
*
* Assumes that "uint8_t" is equivalent to one octet.
* Assumes that "uint32_t" is four octets.
* The ^ operator means exclusive OR.
*/
uint32_t CalcCRC32K(uint32_t crc32kValue, uint8_t dataValue)
{
	uint8_t data, b;
	uint32_t crc;
	data = dataValue;
	crc = crc32kValue;
	for (b = 0; b < 8; b++) {
		if ((data & 1) ^ (crc & 1)) {
			crc >>= 1;
			crc ^= 0xEB31D82E; /* CRC-32K polynomial, 1 + x**1 + ... + x**30 (+ x**32) */
		} else {
			crc >>= 1;
		}
		data >>= 1;
	}
	return crc; /* Return updated crc value */
}
#include <stdio.h>
int main()
{
    uint8_t test[] = "Hello World\n";
    uint8_t test1[] = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"

    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"

    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"

    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x00\x00"
    ;
    uint8_t dst[320];
    int len = cobs_encode1(dst, test1, sizeof(test1)-1, 0x55);
    int i;
    for(i=0; i<len; i++) printf(" %02X", dst[i]);
    printf("\n nxor_encode:\n");
    len = cobs_encode(dst, test1, sizeof(test1)-2);
    for(i=0; i<len; i++) printf(" %02X", dst[i]);
    printf("\n nxor_decode:\n");
    //len = cobs_decode1(dst, dst, len, 0x55);
    len = cobs_decode(dst, dst, len);
    for(i=0; i<len; i++) {
        printf(" %02X", dst[i]);
        if ((i&0xF)==0xF) printf("\n");
    }
    printf("\n cobs_encode(Hello World\\n\\x00):\n");
    len = cobs_encode(dst, test, sizeof(test));
    for(i=0; i<len; i++) printf(" %02X", dst[i]);
    printf("\n cobs_encode(CRC32K):\n");

	uint32_t crc32K = CRC32K_INITIAL_VALUE;
    for (i = 0; i < len; i++){
        crc32K = bacnet_crc32k_update(crc32K, dst[i]); /* See Clause G.3.1 */
    }
    crc32K= ~crc32K;
    printf("\n cobs_decode(CRC32K):%08X\n", crc32K);
	int crc_len = cobs_encode(&dst[len], (uint8_t*)&crc32K, 4);
    for(i=0; i<crc_len; i++) printf(" %02X", dst[i]);
    printf("\n cobs_decode(CRC32K):%08X\n", crc32K);

	uint8_t test3[] =
"\x50\x54\x75\xAA\xAA\x5D\xAA\x45\x52\x68\xAB\x54\xBA\xAA\x14\x14"
"\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14"
"\x14\x17\x17\x17\x17\x17\x17\x17\x17\x17\x17\x17\x17\x17\x17\x17"
"\x17\x17\x17\x17\x16\x16\x16\x16\x16\x16\x16\x16\x16\x16\x16\x16"
"\x16\x16\x16\x16\x16\x16\x16\x11\x11\x11\x11\x11\x11\x11\x11\x11"
"\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x10\x10\x10\x10\x10\x10"
"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x13\x13\x13"
"\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13"
"\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12"
"\x12\x12\x12\x1D\x1D\x1D\x1D\x1D\x1D\x1D\x1D\x1D\x1D\x1D\x1D\x1D"
"\x1D\x1D\x1D\x1D\x1D\x1D\x1C\x1C\x1C\x1C\x1C\x1C\x1C\x1C\x1C\x1C"
"\x1C\x1C\x1C\x1C\x1C\x1C\x1C\x1C\x1C\x1F\x1F\x1F\x1F\x1F\x1F\x1F"
"\x1F\x1F\x1F\x1F\x1F\x1F\x1F\x1F\x1F\x1F\x1F\x1F\x1E\x1E\x1E\x1E"
"\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x19"
"\x19\x19\x19\x19\x19\x19\x19\x19\x19\x19\x19\x19\x19\x19\x19\x19"
"\x19\x19\x18\x18\x18\x18\x18\x18\x18\x18\x18\x18\x18\x18\x18\x18"
"\x18\x18\x18\x18\x18\x1B\x1B\x1B\x1B\x1B\x1B\x1B\xA4\x1B\x1B\x1B"
"\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1A\x1A\x1A\x1A\x1A\x1A\x1A"
"\x1A\x1A\x1A\x1A\x1A\x1A\x1A\x1A\x1A\x1A\x1A\x1A\x05\x05\x05\x05"
"\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x04"
"\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04"
"\x04\x04\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07"
"\x07\x07\x07\x07\x07\x06\x06\x06\x06\x06\x06\x06\x06\x06\x06\x06"
"\x06\x06\x06\x06\x06\x06\x06\x06\x01\x01\x01\x01\x01\x01\x01\x01"
"\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x03"
"\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
"\x03\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02"
"\x02\x02\x02\x02\x0D\x0D\x0D\x0D\x0D\x0D\x0D\x0D\x0D\x0D\x0D\x0D"
"\x0D\x0D\x0D\x0D\x0D\x0D\x0D\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C"
"\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0F\x0F\x0F\x0F\x0F\x0F"
"\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F"
"\x50\xF9\xA1\xD6\xE8";

/*
    printf("\n cobs_encode(Test3):\n");
    len = cobs_decode(dst, test3, sizeof(test3)-6)-dst;
    for(i=0; i<len; i++) {
		printf(" %02X", dst[i]);
	}
*/

	crc32K = CRC32K_INITIAL_VALUE;
    for (i = 0; i < sizeof(test3)-6; i++){
        crc32K = bacnet_crc32k_update(crc32K, test3[i]); /* See Clause G.3.1 */
    }
    crc32K= ~crc32K;
    printf("\n cobs_decode(CRC32K):%08X\n", crc32K);
	crc_len = cobs_encode(&dst[0], (uint8_t*)&crc32K, 4);
    for(i=0; i<crc_len; i++) printf(" %02X", dst[i]);
	printf("\n");

    return 0;
}

#endif
