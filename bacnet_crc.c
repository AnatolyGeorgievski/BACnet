/*! \brief Вычисление контрольной суммы заголовков и данных в BACnet MS/TP */
/*

Description Value CRC Register After Octet is Processed
preamble 1, not included in CRC X'55'
preamble 2, not included in CRC X'FF'
X'FF' (initial value)
frame type = TOKEN X'00' X'55'
destination address X'10' X'C2'
source address X'05' X'BC'
data length MSB = 0 X'00' X'95'
data length LSB = 0 X'00' X'73' ones complement is X'8C'
Header CRC X'8C' X'55' final result at receiver
00 10 05 00 00 8C
01 23 07 00 00 3a


Description Value CRC Accumulator After Octet is Processed
X'FFFF' (initial value)
first data octet X'01' X'1E0E'
second data octet X'22' X'EB70'
third data octet X'30' X'42EF' ones complement is X'BD10'
CRC1 (least significant octet) X'10' X'0F3A'
CRC2 (most significant octet) X'BD' X'F0B8' final result at receiver

Header CRC-8/BACnet: X8 + X7 + 1
	width=8 poly=0x81 init=0xff refin=true refout=true xorout=0xff check=0x89 name="??"

Data CRC-16/X-25: X16 + X12 + X5 + 1
	width=16 poly=0x1021 init=0xffff refin=true refout=true xorout=0xffff check=0x906e name="X-25"
*/
#include <stdint.h>


static uint16_t crc16_table[16]={
0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
0x8408, 0x9489, 0xA50A, 0xB58B, 0xC60C, 0xD68D, 0xE70E, 0xF78F
};


#define POLY16	0x1021
#define POLY16B	0x1081
#define CRC16_MASK 0xFFFF;
static uint16_t	bacnet_crc16_update(uint16_t crc, unsigned char val)
{
	crc = (crc>>4) ^ crc16_table[(crc ^ (val   )) & 0xF];
	crc = (crc>>4) ^ crc16_table[(crc ^ (val>>4)) & 0xF];
	return crc;
}
/* этот вариант не использует таблицу, использует умножение вместо carry-less умножения, потому что полином позволяет это сделать.
uint16_t	bacnet_crc16_update(uint16_t crc, unsigned char val){
	crc = ((crc >> 4) ^ (POLY16B* ((crc ^ (val     )) & 0xF))) & CRC16_MASK;
	crc = ((crc >> 4) ^ (POLY16B* ((crc ^ (val >> 4)) & 0xF))) & CRC16_MASK;
	return crc;
}*/

/*! таблица для расчета CRC-8/BAC
Таблица умножения расcчитана в уме, методом сдвига полинома вправо 0x81 и редуцирования методом (хоr 0x81)
По сути это операция умножения галуа, с обратной последовательностью бит (отражением) бит на входе и выходе.
0*0x81 1*0x81 2*0x81 ... 0xF*0x81
затем элементы массива переставлены так, чтобы в индексе была обратная последовательность бит
0 8 4 C ... F
 */
static uint8_t crc8_table[] =  {
 0x00, 0xF1, 0xE1, 0x10,
 0xC1, 0x30, 0x20, 0xD1,
 0x81, 0x70, 0x60, 0x91,
 0x40, 0xB1, 0xA1, 0x50
};
// 0x00 0xF1 0xE1 0x10 0xC1 0x30 0x20 0xD1 0x81 0x70 0x60 0x91 0x40 0xB1 0xA1 0x50

static uint8_t	bacnet_crc8_update(uint8_t crc, unsigned char val)
{
	crc = (crc>>4) ^ crc8_table[(crc ^ (val   )) & 0xF];
	crc = (crc>>4) ^ crc8_table[(crc ^ (val>>4)) & 0xF];
	return crc;
}
uint8_t	bacnet_crc8(const uint8_t * buffer){
	uint8_t crc8= 0xFF;
	int i;
	for(i=0;i<5;i++)
		crc8 = bacnet_crc8_update (crc8, buffer[i]);
	return crc8 ^ 0xFF;
}
// вычисление контрольной суммы от данных
uint16_t bacnet_crc16 (const uint8_t * buffer, int len)
{
	uint16_t crc = 0xFFFF;
	int i;
	for(i=0;i<len;i++)
		crc = bacnet_crc16_update (crc, buffer[i]);
	return crc ^ 0xFFFF;
}
#if 0
/*! \brief Рассчитывает CRC8 для шапки пакета из 5 байт. 
	Шапка всегда имеет размер 5 байт. 
 */
uint8_t bacnet_crc8(const uint8_t * buffer){

	uint64_t val = *(uint32_t*)buffer | (uint64_t)buffer[4]<<32;
	poly64x2_t c = {val};
	poly64x2_t t = CL_MUL128(c, (poly64x2_t){0x808182878899AAFFULL<<24, 0x103}, 0x00);
	c = CL_MUL128(t, (poly64x2_t){0x808182878899AAFF<<24, 0x103}, 0x10);
	return c[1] ^ 0xFF;// вынести CRC(0xFF) сюда
}
#endif
#ifdef BACNET_TEST_CRC

#include <stdio.h>
void crc_gen_inv_table(uint32_t poly, int bits)
{
	uint32_t table[16] = {0};
	uint32_t p =poly;
	int i,j;
	table[0] = 0;
	table[1] = p;
	for (i=1;(1<<i)<16;i++)
	{
		if (p&1)
			p = (p>>1) ^ poly;
		else
			p = (p>>1);
		table[(1<<i)] = p;
		for(j=1; j<(1<<i); j++) {
			table[(1<<i)+j] = p ^ table[j];
		}
	}
	printf("POLY=0x%0*X\n", bits/4, poly);
	for(i=0;i<16;i++){
		int ri;
		ri = ( i&0x3)<<2 | ( i&0xC)>>2;
		ri = (ri&0x5)<<1 | (ri&0xA)>>1;
		printf("0x%0*X, ", bits/4, table[ri]);
		if ((i&0x7)==0x7) printf("\n");
	}
	printf("\n");
}

void crc_gen_table(uint32_t poly, int bits, int size)
{
	uint32_t table[size];// = {0};
	uint32_t p =poly;
	int i,j;
	table[0] = 0;
	table[1] = p;
	for (i=1;(1<<i)<size;i++)
	{
		if (p&(1<<(bits-1))) {
			p &= ~((~0)<<(bits-1));
			p = (p<<1) ^ poly;
		} else
			p = (p<<1);
		table[(1<<i)] = p;
		for(j=1; j<(1<<i); j++) {
			table[(1<<i)+j] = p ^ table[j];
		}
	}
	printf("POLY=0x%0*X\n", bits/4, poly);
	for(i=0;i<size;i++){
		printf("0x%0*X, ", bits/4, table[i]);
		if ((i&0x7)==0x7) printf("\n");
	}
	printf("\n");
}
int main()
{
	crc_gen_inv_table(0xEB31D82E,32);
	crc_gen_table(0x04C11DB7,32,16);
	crc_gen_table(0x04C11DB7,32,256);
	crc_gen_inv_table(0x8408,16);
	crc_gen_inv_table(0xA001,16); // - modbus
	crc_gen_inv_table(0x81,8);
	crc_gen_table(0x81,8,16); // прямая
	//uint8_t data[] = {0x01,0x22,0x30, 0x10,0xbd};
	uint8_t dat1[] ={
		0x05,0x07,0x00,0x00, 0x16,0x2b,0x01,0x0c,
		0x00,0x01,0x06,0xc0, 0xa8,0x00,0x69,0xba,
		0xc0,0x00,0x05,0x10, 0x0c,0x0c,0x02,0x00,
		0x00,0x07,0x19,0x4c, 0x83,0xf9};
	int i;
	uint16_t crc= 0xFFFF;
	for(i=6;i<28;i++)
		crc = bacnet_crc16_update (crc, dat1[i]);
	crc ^= 0xFFFF;
	printf("%04X CRC\n", crc);// lsb-msb

	uint8_t *data = "123456789";
	crc= 0xFFFF;
	for(i=0;i<9;i++)
		crc = bacnet_crc16_update (crc, data[i]);
	crc ^= 0xFFFF;
	printf("%04X CHK\n", crc);

	//uint8_t dat2[] = {0x00,0x10,0x05,0x00,0x00,0x8C};
	//uint8_t dat2[] = {0x01,0x23,0x07,0x00,0x00,0x3a};
	//uint8_t dat2[] = {0x06,0xff,0x07,0x00,0x14,0x25};
	//uint8_t dat2[] = {0x06,0x00,0x07,0x00,0x20,0xc7};
	uint8_t dat2[] = {0x00,0x00,0x7e,0x00,0x00,0xa7};
	uint8_t crc8= 0xFF;
	for(i=0;i<5;i++)
		crc8 = bacnet_crc8_update (crc8, dat2[i]);
	crc8 ^= 0xFF;
	printf("%02X CRC\n", crc8);
	crc8= 0xFF;
	for(i=0;i<9;i++)
		crc8 = bacnet_crc8_update (crc8, data[i]);
	crc8 ^= 0xFF;
	printf("%02X CHK\n", crc8);


	uint8_t crc81= 0xFF;
	uint8_t crc82= 0x00;
	for(i=0;i<9;i++) {
		crc81 = bacnet_crc8_update (crc81, data[i] & 0xF0);
		crc82 = bacnet_crc8_update (crc82, data[i] & 0x0F);
	}
	crc8 = crc81 ^ crc82 ^ 0xFF;
	printf("%02X CHK\n", crc8);


	printf("MUL %02X %02X  CHK\n", crc8_table[0x5] ^ crc8_table[0xA], crc8_table[0xF]);

	uint8_t dat3[] = {0x01,0x22,0x30, 0xBE,0xA5,0x22,0x7C};//7C22A5BE
	uint32_t crc32= 0xFFFFFFFF;
	for(i=0;i<3;i++)
		crc32 = bacnet_crc32k_update (crc32, dat3[i]);
	crc32 ^= 0xFFFFFFFF;
	printf("%08X CRC\n", crc32);
	crc32= 0xFFFFFFFF;
	for(i=0;i<9;i++)
		crc32 = bacnet_crc32k_update (crc32, data[i]);
	crc32 ^= 0xFFFFFFFF;
	printf("CRC32K: %08X CHK!!\n", crc32);


	uint32_t crc321= 0xFFFFFFFF;
	uint32_t crc322= 0;
	for(i=0;i<9;i++) {
		crc321 = bacnet_crc32k_update (crc321, data[i] & 0xF0);
		crc322 = bacnet_crc32k_update (crc322, data[i] & 0x0F);
	}
	crc32 = crc321 ^ crc322 ^ 0xFFFFFFFF;
	printf("%08X CHK\n", crc32);

	return 0;
}
#endif
