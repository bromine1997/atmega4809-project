#ifndef D24FC512_H_
#define D24FC512_H_

#define D24FC512_ADDR		0x50

uint8_t EEPROM_ReadByte( uint16_t adr );
uint16_t EEPROM_ReadUint16( uint16_t adr );
void EEPROM_ReadBlock( uint16_t adr, uint8_t *buffer, uint8_t len );

void EEPROM_WriteByte( uint16_t adr, uint8_t dat );
void EEPROM_WriteUint16( uint16_t adr, uint16_t dat );
void EEPROM_WritePage( uint16_t adr, uint8_t *buffer, uint8_t len );
void EEPROM_WriteBlock( uint16_t adr, uint8_t *buffer, uint8_t len );
void EEPROM_WriteAnyBlock( uint16_t adr, uint8_t *pbytes, uint16_t siz );

#endif /* 24FC512_H_ */