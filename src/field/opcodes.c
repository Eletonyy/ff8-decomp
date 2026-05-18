/* Field script-VM opcode dispatch table.
 * Indexed by the opcode byte read from the bytecode stream.
 * Index 19 (func_800AE048) is the "extended opcode" — re-dispatches
 * with a second byte as the table index.
 */
#include "common.h"
#include "field.h"

/* Forward declarations for the 392 opcode handlers (K&R-style
 * unprototyped — the table cast handles arg-type compatibility). */
extern void func_800ADCA4();
extern void func_800ADCD8();
extern void func_800ADD30();
extern void func_800ADD68();
extern void func_800ADDA0();
extern void func_800ADD0C();
extern void func_800ADDD8();
extern void func_800ADE10();
extern void func_800ADE44();
extern void func_800ADE7C();
extern void func_800ADEB0();
extern void func_800ADEE8();
extern void func_800ADF20();
extern void func_800ADF54();
extern void func_800ADF88();
extern void func_800ADFBC();
extern void func_800ADFE0();
extern void func_800AE014();
extern void func_800ADC9C();
extern void func_800AE048();
extern void func_800AE080();
extern void func_800AE098();
extern void func_800AE0DC();
extern void func_800AE124();
extern void func_800AE1AC();
extern void func_800AE4C4();
extern void func_800AE518();
extern void func_800AE7B4();
extern void func_800AE574();
extern void func_800AE7E4();
extern void func_800AE5D4();
extern void func_800AE81C();
extern void func_800AE634();
extern void func_800AE854();
extern void func_800AE694();
extern void func_800AE6F4();
extern void func_800AE754();
extern void func_800AE88C();
extern void func_800AE978();
extern void func_800AEA44();
extern void func_800AEB0C();
extern void func_800AEBD8();
extern void func_800AEC78();
extern void func_800AED9C();
extern void func_800AF5F8();
extern void func_800AEEC4();
extern void func_800AEECC();
extern void func_800AEED4();
extern void func_800AEF4C();
extern void func_800AEFE8();
extern void func_800AF02C();
extern void func_800B2348();
extern void func_800B2A40();
extern void func_800B8344();
extern void func_800B83FC();
extern void func_800B85F8();
extern void func_800B8710();
extern void func_800B8824();
extern void func_800B89C0();
extern void func_800B460C();
extern void func_800B46E4();
extern void func_800AF114();
extern void func_800AF224();
extern void func_800B9604();
extern void func_800B9678();
extern void func_800B96EC();
extern void func_800B9798();
extern void func_800B9844();
extern void func_800B9888();
extern void func_800B98CC();
extern void func_800B9944();
extern void func_800B99BC();
extern void func_800B9A00();
extern void func_800AF274();
extern void func_800B47E4();
extern void func_800AF4C4();
extern void func_800AF5A8();
extern void func_800AF5B8();
extern void func_800AF070();
extern void func_800B68B8();
extern void opHandler_MES();
extern void func_800B69E8();
extern void func_800B6B20();
extern void func_800B6C28();
extern void func_800B6D24();
extern void func_800B84D8();
extern void func_800B95A0();
extern void func_800B95C0();
extern void func_800BC034();
extern void func_800BC170();
extern void func_800BCB14();
extern void func_800BBEA4();
extern void func_800BC6F0();
extern void func_800BCCAC();
extern void func_800BCDA0();
extern void func_800AF610();
extern void func_800AF7E4();
extern void func_800B13EC();
extern void func_800B158C();
extern void func_800AFD68();
extern void func_800B9F58();
extern void func_800B9F88();
extern void func_800BA034();
extern void func_800BA09C();
extern void func_800B15BC();
extern void func_800B0B04();
extern void func_800B0B10();
extern void func_800B0B20();
extern void func_800B0B2C();
extern void func_800B0C64();
extern void func_800B48EC();
extern void func_800B497C();
extern void func_800B498C();
extern void func_800BBFFC();
extern void func_800B0A08();
extern void func_800B0A7C();
extern void func_800B0CCC();
extern void func_800B0CFC();
extern void func_800BC2E0();
extern void func_800BC44C();
extern void func_800B0D2C();
extern void func_800B0C48();
extern void func_800B0C58();
extern void func_800B64B0();
extern void func_800B6524();
extern void func_800B653C();
extern void func_800B6588();
extern void func_800AF2C4();
extern void func_800AF314();
extern void func_800BC8CC();
extern void func_800B0D94();
extern void func_800B348C();
extern void func_800B34EC();
extern void func_800B3574();
extern void func_800B35FC();
extern void func_800B3650();
extern void func_800B36C8();
extern void func_800B3868();
extern void func_800B7050();
extern void func_800B711C();
extern void func_800B7228();
extern void func_800B7310();
extern void func_800B73D4();
extern void func_800B7490();
extern void func_800AF0B4();
extern void func_800B3740();
extern void func_800B3788();
extern void func_800B37F8();
extern void func_800BA424();
extern void func_800BA4D4();
extern void func_800BA584();
extern void func_800BA634();
extern void func_800AFE24();
extern void func_800AFF64();
extern void func_800B002C();
extern void func_800B0280();
extern void func_800B0124();
extern void func_800B02A0();
extern void func_800B0344();
extern void func_800B0444();
extern void func_800B0570();
extern void func_800BA6E4();
extern void func_800BA7DC();
extern void func_800BA8D4();
extern void func_800BA9E8();
extern void func_800B79C8();
extern void func_800BCC6C();
extern void func_800B2DC0();
extern void func_800B2E68();
extern void func_800B2EDC();
extern void func_800B2F50();
extern void func_800B2F70();
extern void func_800B2FD8();
extern void func_800B301C();
extern void func_800B417C();
extern void func_800B41CC();
extern void func_800B42E0();
extern void func_800B9C58();
extern void func_800B9CBC();
extern void func_800B1738();
extern void func_800B17A0();
extern void func_800B12A4();
extern void func_800B41B0();
extern void func_800BB768();
extern void func_800BB7BC();
extern void func_800BBDE0();
extern void func_800B33B8();
extern void func_800B3474();
extern void func_800BBE78();
extern void func_800B0B3C();
extern void func_800B0BE4();
extern void func_800B65B8();
extern void func_800B65CC();
extern void func_800B9F28();
extern void func_800B9D7C();
extern void func_800B6E18();
extern void func_800B6F4C();
extern void func_800B9D20();
extern void func_800B1A20();
extern void func_800B18A4();
extern void func_800BBE50();
extern void func_800B9A78();
extern void func_800B9B24();
extern void func_800B4288();
extern void func_800B1C7C();
extern void func_800B1D40();
extern void func_800B22C0();
extern void func_800B2248();
extern void func_800B17A8();
extern void func_800B1E34();
extern void func_800B1F48();
extern void func_800B1FE0();
extern void func_800B2090();
extern void func_800B23F4();
extern void func_800B2434();
extern void func_800B248C();
extern void func_800B24CC();
extern void func_800B2524();
extern void func_800B257C();
extern void func_800B25F0();
extern void func_800B2648();
extern void func_800B1870();
extern void func_800B6478();
extern void func_800B26BC();
extern void func_800B3050();
extern void func_800B17D8();
extern void func_800B3080();
extern void func_800B3334();
extern void func_800B31B4();
extern void func_800B38E0();
extern void func_800B3964();
extern void func_800B3A04();
extern void func_800B3AA4();
extern void func_800B3B20();
extern void func_800B3BC0();
extern void func_800B3C60();
extern void func_800B3CD0();
extern void func_800B3D60();
extern void func_800B388C();
extern void func_800B3DF0();
extern void func_800B49D8();
extern void func_800B49C4();
extern void func_800B2AC0();
extern void func_800B2AD8();
extern void func_800B2AF0();
extern void func_800B2B34();
extern void func_800B44BC();
extern void func_800AF5E0();
extern void func_800B7E78();
extern void func_800B9570();
extern void func_800AF0E0();
extern void func_800BB810();
extern void func_800BB8B4();
extern void func_800BB958();
extern void func_800BBA3C();
extern void func_800BBB20();
extern void func_800BBC64();
extern void func_800BBDA8();
extern void func_800B8B58();
extern void func_800B8BE0();
extern void func_800B8CD4();
extern void func_800B8DC8();
extern void func_800B8E74();
extern void func_800B8F20();
extern void func_800B8F50();
extern void func_800B8F3C();
extern void func_800B49A0();
extern void func_800B49B4();
extern void func_800B8F60();
extern void func_800AF120();
extern void func_800AF1AC();
extern void func_800BAD00();
extern void func_800BADCC();
extern void func_800BAF14();
extern void func_800BB5E0();
extern void func_800BB05C();
extern void func_800AF404();
extern void func_800AF444();
extern void func_800AF47C();
extern void func_800AF4A0();
extern void func_800BBEFC();
extern void func_800BB650();
extern void func_800BAC38();
extern void func_800B0EBC();
extern void func_800B43FC();
extern void func_800B65B0();
extern void func_800B2A70();
extern void func_800B4320();
extern void func_800BB140();
extern void func_800BB1F0();
extern void func_800BB2A4();
extern void func_800BB3D8();
extern void func_800BB510();
extern void func_800BAC18();
extern void func_800B1034();
extern void func_800B10F8();
extern void func_800BC58C();
extern void func_800B3350();
extern void func_800B8FA8();
extern void func_800B8F80();
extern void func_800B49E8();
extern void func_800B4A18();
extern void func_800B414C();
extern void func_800B0304();
extern void func_800B4A40();
extern void func_800B65D4();
extern void func_800B3F18();
extern void func_800B3F9C();
extern void func_800B4074();
extern void func_800B7718();
extern void func_800B7674();
extern void func_800B5188();
extern void func_800B63A4();
extern void func_800B6400();
extern void func_800B08CC();
extern void func_800B4A88();
extern void func_800B4DBC();
extern void func_800B74B0();
extern void func_800B7578();
extern void func_800B11BC();
extern void func_800B4EB0();
extern void func_800B4ED8();
extern void func_800B4FF8();
extern void func_800B0924();
extern void func_800B1730();
extern void func_800B6448();
extern void func_800B448C();
extern void func_800B1BB8();
extern void func_800B5318();
extern void func_800B5A30();
extern void func_800B4F80();
extern void func_800AFD20();
extern void func_800B5480();
extern void func_800BCE44();
extern void func_800BCECC();
extern void func_800BD1A4();
extern void func_800B3EA0();
extern void func_800B0E68();
extern void func_800B1ED4();
extern void func_800B1AA0();
extern void func_800AF5D4();
extern void func_800AF5C4();
extern void func_800B1B10();
extern void func_800B4DDC();
extern void func_800B4DFC();
extern void func_800B4E60();
extern void func_800B7640();
extern void func_800B2158();
extern void func_800B9488();
extern void func_800B94C0();
extern void func_800AF364();
extern void func_800AF3B4();
extern void func_800B62E8();
extern void func_800B6328();
extern void func_800B1DF4();
extern void func_800B51E0();
extern void func_800B5248();
extern void func_800B52B0();
extern void func_800B6364();
extern void func_800B6210();
extern void func_800B4D7C();
extern void func_800B0784();
extern void func_800B0818();
extern void func_800B085C();
extern void func_800B4D0C();
extern void func_800B9030();
extern void func_800B9078();
extern void func_800B90C0();
extern void func_800B536C();
extern void func_800B53D8();
extern void func_800B542C();
extern void func_800B505C();
extern void func_800B06D0();
extern void func_800B0638();
extern void func_800B16B0();
extern void func_800BD024();
extern void func_800BD1F4();
extern void func_800BD2AC();
extern void func_800BD318();
extern void func_800B9000();
extern void func_800B5134();
extern void func_800BA120();
extern void func_800BA1D0();
extern void func_800BA280();
extern void func_800BA330();
extern void func_800B1F04();
extern void func_800BAAFC();
extern void func_800BABFC();
extern void func_800B273C();
extern void func_800B2790();
extern void func_800B85C8();
extern void func_800B629C();

typedef s32 (*OpcodeFn)(Eline *);

s32 (*D_800C6760[392])(Eline *) = {
    /* 0x000 */ (OpcodeFn)func_800ADCA4,
    /* 0x001 */ (OpcodeFn)func_800ADCD8,
    /* 0x002 */ (OpcodeFn)func_800ADD30,
    /* 0x003 */ (OpcodeFn)func_800ADD68,
    /* 0x004 */ (OpcodeFn)func_800ADDA0,
    /* 0x005 */ (OpcodeFn)func_800ADD0C,
    /* 0x006 */ (OpcodeFn)func_800ADDD8,
    /* 0x007 */ (OpcodeFn)func_800ADE10,
    /* 0x008 */ (OpcodeFn)func_800ADE44,
    /* 0x009 */ (OpcodeFn)func_800ADE7C,
    /* 0x00A */ (OpcodeFn)func_800ADEB0,
    /* 0x00B */ (OpcodeFn)func_800ADEE8,
    /* 0x00C */ (OpcodeFn)func_800ADF20,
    /* 0x00D */ (OpcodeFn)func_800ADF54,
    /* 0x00E */ (OpcodeFn)func_800ADF88,
    /* 0x00F */ (OpcodeFn)func_800ADFBC,
    /* 0x010 */ (OpcodeFn)func_800ADFE0,
    /* 0x011 */ (OpcodeFn)func_800AE014,
    /* 0x012 */ (OpcodeFn)func_800ADC9C,
    /* 0x013 */ (OpcodeFn)func_800AE048,
    /* 0x014 */ (OpcodeFn)func_800AE080,
    /* 0x015 */ (OpcodeFn)func_800AE098,
    /* 0x016 */ (OpcodeFn)func_800AE0DC,
    /* 0x017 */ (OpcodeFn)func_800AE124,
    /* 0x018 */ (OpcodeFn)func_800AE1AC,
    /* 0x019 */ (OpcodeFn)func_800AE4C4,
    /* 0x01A */ (OpcodeFn)func_800AE518,
    /* 0x01B */ (OpcodeFn)func_800AE7B4,
    /* 0x01C */ (OpcodeFn)func_800AE574,
    /* 0x01D */ (OpcodeFn)func_800AE7E4,
    /* 0x01E */ (OpcodeFn)func_800AE5D4,
    /* 0x01F */ (OpcodeFn)func_800AE81C,
    /* 0x020 */ (OpcodeFn)func_800AE634,
    /* 0x021 */ (OpcodeFn)func_800AE854,
    /* 0x022 */ (OpcodeFn)func_800AE694,
    /* 0x023 */ (OpcodeFn)func_800AE6F4,
    /* 0x024 */ (OpcodeFn)func_800AE754,
    /* 0x025 */ (OpcodeFn)func_800AE88C,
    /* 0x026 */ (OpcodeFn)func_800AE978,
    /* 0x027 */ (OpcodeFn)func_800AEA44,
    /* 0x028 */ (OpcodeFn)func_800AEB0C,
    /* 0x029 */ (OpcodeFn)func_800AEBD8,
    /* 0x02A */ (OpcodeFn)func_800AEC78,
    /* 0x02B */ (OpcodeFn)func_800AED9C,
    /* 0x02C */ (OpcodeFn)func_800AF5F8,
    /* 0x02D */ (OpcodeFn)func_800AEEC4,
    /* 0x02E */ (OpcodeFn)func_800AEECC,
    /* 0x02F */ (OpcodeFn)func_800AEED4,
    /* 0x030 */ (OpcodeFn)func_800AEF4C,
    /* 0x031 */ (OpcodeFn)func_800AEFE8,
    /* 0x032 */ (OpcodeFn)func_800AF02C,
    /* 0x033 */ (OpcodeFn)func_800B2348,
    /* 0x034 */ (OpcodeFn)func_800B2A40,
    /* 0x035 */ (OpcodeFn)func_800B8344,
    /* 0x036 */ (OpcodeFn)func_800B83FC,
    /* 0x037 */ (OpcodeFn)func_800B85F8,
    /* 0x038 */ (OpcodeFn)func_800B8710,
    /* 0x039 */ (OpcodeFn)func_800B8824,
    /* 0x03A */ (OpcodeFn)func_800B89C0,
    /* 0x03B */ (OpcodeFn)func_800B460C,
    /* 0x03C */ (OpcodeFn)func_800B46E4,
    /* 0x03D */ (OpcodeFn)func_800AF114,
    /* 0x03E */ (OpcodeFn)func_800AF224,
    /* 0x03F */ (OpcodeFn)func_800B9604,
    /* 0x040 */ (OpcodeFn)func_800B9678,
    /* 0x041 */ (OpcodeFn)func_800B96EC,
    /* 0x042 */ (OpcodeFn)func_800B9798,
    /* 0x043 */ (OpcodeFn)func_800B9844,
    /* 0x044 */ (OpcodeFn)func_800B9888,
    /* 0x045 */ (OpcodeFn)func_800B98CC,
    /* 0x046 */ (OpcodeFn)func_800B9944,
    /* 0x047 */ (OpcodeFn)func_800B99BC,
    /* 0x048 */ (OpcodeFn)func_800B9A00,
    /* 0x049 */ (OpcodeFn)func_800AF274,
    /* 0x04A */ (OpcodeFn)func_800B47E4,
    /* 0x04B */ (OpcodeFn)func_800AF4C4,
    /* 0x04C */ (OpcodeFn)func_800AF5A8,
    /* 0x04D */ (OpcodeFn)func_800AF5B8,
    /* 0x04E */ (OpcodeFn)func_800AF070,
    /* 0x04F */ (OpcodeFn)func_800B68B8,
    /* 0x050 */ (OpcodeFn)opHandler_MES,
    /* 0x051 */ (OpcodeFn)func_800B69E8,
    /* 0x052 */ (OpcodeFn)func_800B6B20,
    /* 0x053 */ (OpcodeFn)func_800B6C28,
    /* 0x054 */ (OpcodeFn)func_800B6D24,
    /* 0x055 */ (OpcodeFn)func_800B84D8,
    /* 0x056 */ (OpcodeFn)func_800B95A0,
    /* 0x057 */ (OpcodeFn)func_800B95C0,
    /* 0x058 */ (OpcodeFn)func_800BC034,
    /* 0x059 */ (OpcodeFn)func_800BC170,
    /* 0x05A */ (OpcodeFn)func_800BCB14,
    /* 0x05B */ (OpcodeFn)func_800BBEA4,
    /* 0x05C */ (OpcodeFn)func_800BC6F0,
    /* 0x05D */ (OpcodeFn)func_800BCCAC,
    /* 0x05E */ (OpcodeFn)func_800BCDA0,
    /* 0x05F */ (OpcodeFn)func_800AF610,
    /* 0x060 */ (OpcodeFn)func_800AF7E4,
    /* 0x061 */ (OpcodeFn)func_800B13EC,
    /* 0x062 */ (OpcodeFn)func_800B158C,
    /* 0x063 */ (OpcodeFn)func_800AFD68,
    /* 0x064 */ (OpcodeFn)func_800B9F58,
    /* 0x065 */ (OpcodeFn)func_800B9F88,
    /* 0x066 */ (OpcodeFn)func_800BA034,
    /* 0x067 */ (OpcodeFn)func_800BA09C,
    /* 0x068 */ (OpcodeFn)func_800B15BC,
    /* 0x069 */ (OpcodeFn)func_800B0B04,
    /* 0x06A */ (OpcodeFn)func_800B0B10,
    /* 0x06B */ (OpcodeFn)func_800B0B20,
    /* 0x06C */ (OpcodeFn)func_800B0B2C,
    /* 0x06D */ (OpcodeFn)func_800B0C64,
    /* 0x06E */ (OpcodeFn)func_800B48EC,
    /* 0x06F */ (OpcodeFn)func_800B497C,
    /* 0x070 */ (OpcodeFn)func_800B498C,
    /* 0x071 */ (OpcodeFn)func_800BBFFC,
    /* 0x072 */ (OpcodeFn)func_800B0A08,
    /* 0x073 */ (OpcodeFn)func_800B0A7C,
    /* 0x074 */ (OpcodeFn)func_800B0CCC,
    /* 0x075 */ (OpcodeFn)func_800B0CFC,
    /* 0x076 */ (OpcodeFn)func_800BC2E0,
    /* 0x077 */ (OpcodeFn)func_800BC44C,
    /* 0x078 */ (OpcodeFn)func_800B0D2C,
    /* 0x079 */ (OpcodeFn)func_800B0C48,
    /* 0x07A */ (OpcodeFn)func_800B0C58,
    /* 0x07B */ (OpcodeFn)func_800B64B0,
    /* 0x07C */ (OpcodeFn)func_800B6524,
    /* 0x07D */ (OpcodeFn)func_800B653C,
    /* 0x07E */ (OpcodeFn)func_800B6588,
    /* 0x07F */ (OpcodeFn)func_800AF2C4,
    /* 0x080 */ (OpcodeFn)func_800AF314,
    /* 0x081 */ (OpcodeFn)func_800BC8CC,
    /* 0x082 */ (OpcodeFn)func_800B0D94,
    /* 0x083 */ (OpcodeFn)func_800B348C,
    /* 0x084 */ (OpcodeFn)func_800B34EC,
    /* 0x085 */ (OpcodeFn)func_800B3574,
    /* 0x086 */ (OpcodeFn)func_800B35FC,
    /* 0x087 */ (OpcodeFn)func_800B3650,
    /* 0x088 */ (OpcodeFn)func_800B36C8,
    /* 0x089 */ (OpcodeFn)func_800B3868,
    /* 0x08A */ (OpcodeFn)func_800B7050,
    /* 0x08B */ (OpcodeFn)func_800B711C,
    /* 0x08C */ (OpcodeFn)func_800B7228,
    /* 0x08D */ (OpcodeFn)func_800B7310,
    /* 0x08E */ (OpcodeFn)func_800B73D4,
    /* 0x08F */ (OpcodeFn)func_800B7490,
    /* 0x090 */ (OpcodeFn)func_800AF0B4,
    /* 0x091 */ (OpcodeFn)func_800B3740,
    /* 0x092 */ (OpcodeFn)func_800B3788,
    /* 0x093 */ (OpcodeFn)func_800B37F8,
    /* 0x094 */ (OpcodeFn)func_800BA424,
    /* 0x095 */ (OpcodeFn)func_800BA4D4,
    /* 0x096 */ (OpcodeFn)func_800BA584,
    /* 0x097 */ (OpcodeFn)func_800BA634,
    /* 0x098 */ (OpcodeFn)func_800AFE24,
    /* 0x099 */ (OpcodeFn)func_800AFF64,
    /* 0x09A */ (OpcodeFn)func_800B002C,
    /* 0x09B */ (OpcodeFn)func_800B0280,
    /* 0x09C */ (OpcodeFn)func_800B0124,
    /* 0x09D */ (OpcodeFn)func_800B02A0,
    /* 0x09E */ (OpcodeFn)func_800B0344,
    /* 0x09F */ (OpcodeFn)func_800B0444,
    /* 0x0A0 */ (OpcodeFn)func_800B0570,
    /* 0x0A1 */ (OpcodeFn)func_800BA6E4,
    /* 0x0A2 */ (OpcodeFn)func_800BA7DC,
    /* 0x0A3 */ (OpcodeFn)func_800BA8D4,
    /* 0x0A4 */ (OpcodeFn)func_800BA9E8,
    /* 0x0A5 */ (OpcodeFn)func_800B79C8,
    /* 0x0A6 */ (OpcodeFn)func_800BCC6C,
    /* 0x0A7 */ (OpcodeFn)func_800B2DC0,
    /* 0x0A8 */ (OpcodeFn)func_800B2E68,
    /* 0x0A9 */ (OpcodeFn)func_800B2EDC,
    /* 0x0AA */ (OpcodeFn)func_800B2F50,
    /* 0x0AB */ (OpcodeFn)func_800B2F70,
    /* 0x0AC */ (OpcodeFn)func_800B2FD8,
    /* 0x0AD */ (OpcodeFn)func_800B301C,
    /* 0x0AE */ (OpcodeFn)func_800B417C,
    /* 0x0AF */ (OpcodeFn)func_800B41CC,
    /* 0x0B0 */ (OpcodeFn)func_800B42E0,
    /* 0x0B1 */ (OpcodeFn)func_800B9C58,
    /* 0x0B2 */ (OpcodeFn)func_800B9CBC,
    /* 0x0B3 */ (OpcodeFn)func_800B1738,
    /* 0x0B4 */ (OpcodeFn)func_800B17A0,
    /* 0x0B5 */ (OpcodeFn)func_800B12A4,
    /* 0x0B6 */ (OpcodeFn)func_800B41B0,
    /* 0x0B7 */ (OpcodeFn)func_800BB768,
    /* 0x0B8 */ (OpcodeFn)func_800BB7BC,
    /* 0x0B9 */ (OpcodeFn)func_800BBDE0,
    /* 0x0BA */ (OpcodeFn)func_800B33B8,
    /* 0x0BB */ (OpcodeFn)func_800B3474,
    /* 0x0BC */ (OpcodeFn)func_800BBE78,
    /* 0x0BD */ (OpcodeFn)func_800B0B3C,
    /* 0x0BE */ (OpcodeFn)func_800B0BE4,
    /* 0x0BF */ (OpcodeFn)func_800B65B8,
    /* 0x0C0 */ (OpcodeFn)func_800B65CC,
    /* 0x0C1 */ (OpcodeFn)func_800B9F28,
    /* 0x0C2 */ (OpcodeFn)func_800B9D7C,
    /* 0x0C3 */ (OpcodeFn)func_800B6E18,
    /* 0x0C4 */ (OpcodeFn)func_800B6F4C,
    /* 0x0C5 */ (OpcodeFn)func_800B9D20,
    /* 0x0C6 */ (OpcodeFn)func_800B1A20,
    /* 0x0C7 */ (OpcodeFn)func_800B18A4,
    /* 0x0C8 */ (OpcodeFn)func_800BBE50,
    /* 0x0C9 */ (OpcodeFn)func_800B9A78,
    /* 0x0CA */ (OpcodeFn)func_800B9B24,
    /* 0x0CB */ (OpcodeFn)func_800B4288,
    /* 0x0CC */ (OpcodeFn)func_800B1C7C,
    /* 0x0CD */ (OpcodeFn)func_800B1D40,
    /* 0x0CE */ (OpcodeFn)func_800B22C0,
    /* 0x0CF */ (OpcodeFn)func_800B2248,
    /* 0x0D0 */ (OpcodeFn)func_800B17A8,
    /* 0x0D1 */ (OpcodeFn)func_800B1E34,
    /* 0x0D2 */ (OpcodeFn)func_800B1F48,
    /* 0x0D3 */ (OpcodeFn)func_800B1FE0,
    /* 0x0D4 */ (OpcodeFn)func_800B2090,
    /* 0x0D5 */ (OpcodeFn)func_800B23F4,
    /* 0x0D6 */ (OpcodeFn)func_800B2434,
    /* 0x0D7 */ (OpcodeFn)func_800B248C,
    /* 0x0D8 */ (OpcodeFn)func_800B24CC,
    /* 0x0D9 */ (OpcodeFn)func_800B2524,
    /* 0x0DA */ (OpcodeFn)func_800B257C,
    /* 0x0DB */ (OpcodeFn)func_800B25F0,
    /* 0x0DC */ (OpcodeFn)func_800B2648,
    /* 0x0DD */ (OpcodeFn)func_800B1870,
    /* 0x0DE */ (OpcodeFn)func_800B6478,
    /* 0x0DF */ (OpcodeFn)func_800B26BC,
    /* 0x0E0 */ (OpcodeFn)func_800B3050,
    /* 0x0E1 */ (OpcodeFn)func_800B17D8,
    /* 0x0E2 */ (OpcodeFn)func_800B3080,
    /* 0x0E3 */ (OpcodeFn)func_800B3334,
    /* 0x0E4 */ (OpcodeFn)func_800B31B4,
    /* 0x0E5 */ (OpcodeFn)func_800B38E0,
    /* 0x0E6 */ (OpcodeFn)func_800B3964,
    /* 0x0E7 */ (OpcodeFn)func_800B3A04,
    /* 0x0E8 */ (OpcodeFn)func_800B3AA4,
    /* 0x0E9 */ (OpcodeFn)func_800B3B20,
    /* 0x0EA */ (OpcodeFn)func_800B3BC0,
    /* 0x0EB */ (OpcodeFn)func_800B3C60,
    /* 0x0EC */ (OpcodeFn)func_800B3CD0,
    /* 0x0ED */ (OpcodeFn)func_800B3D60,
    /* 0x0EE */ (OpcodeFn)func_800B388C,
    /* 0x0EF */ (OpcodeFn)func_800B3DF0,
    /* 0x0F0 */ (OpcodeFn)func_800B49D8,
    /* 0x0F1 */ (OpcodeFn)func_800B49C4,
    /* 0x0F2 */ (OpcodeFn)func_800B2AC0,
    /* 0x0F3 */ (OpcodeFn)func_800B2AD8,
    /* 0x0F4 */ (OpcodeFn)func_800B2AF0,
    /* 0x0F5 */ (OpcodeFn)func_800B2B34,
    /* 0x0F6 */ (OpcodeFn)func_800B44BC,
    /* 0x0F7 */ (OpcodeFn)func_800AF5E0,
    /* 0x0F8 */ (OpcodeFn)func_800B7E78,
    /* 0x0F9 */ (OpcodeFn)func_800B9570,
    /* 0x0FA */ (OpcodeFn)func_800AF0E0,
    /* 0x0FB */ (OpcodeFn)func_800BB810,
    /* 0x0FC */ (OpcodeFn)func_800BB8B4,
    /* 0x0FD */ (OpcodeFn)func_800BB958,
    /* 0x0FE */ (OpcodeFn)func_800BBA3C,
    /* 0x0FF */ (OpcodeFn)func_800BBB20,
    /* 0x100 */ (OpcodeFn)func_800BBC64,
    /* 0x101 */ (OpcodeFn)func_800BBDA8,
    /* 0x102 */ (OpcodeFn)func_800B8B58,
    /* 0x103 */ (OpcodeFn)func_800B8BE0,
    /* 0x104 */ (OpcodeFn)func_800B8CD4,
    /* 0x105 */ (OpcodeFn)func_800B8DC8,
    /* 0x106 */ (OpcodeFn)func_800B8E74,
    /* 0x107 */ (OpcodeFn)func_800B8F20,
    /* 0x108 */ (OpcodeFn)func_800B8F50,
    /* 0x109 */ (OpcodeFn)func_800B8F3C,
    /* 0x10A */ (OpcodeFn)func_800B49A0,
    /* 0x10B */ (OpcodeFn)func_800B49B4,
    /* 0x10C */ (OpcodeFn)func_800B8F60,
    /* 0x10D */ (OpcodeFn)func_800AF120,
    /* 0x10E */ (OpcodeFn)func_800AF1AC,
    /* 0x10F */ (OpcodeFn)func_800BAD00,
    /* 0x110 */ (OpcodeFn)func_800BADCC,
    /* 0x111 */ (OpcodeFn)func_800BAF14,
    /* 0x112 */ (OpcodeFn)func_800BB5E0,
    /* 0x113 */ (OpcodeFn)func_800BB05C,
    /* 0x114 */ (OpcodeFn)func_800AF404,
    /* 0x115 */ (OpcodeFn)func_800AF444,
    /* 0x116 */ (OpcodeFn)func_800AF47C,
    /* 0x117 */ (OpcodeFn)func_800AF4A0,
    /* 0x118 */ (OpcodeFn)func_800BBEFC,
    /* 0x119 */ (OpcodeFn)func_800BB650,
    /* 0x11A */ (OpcodeFn)func_800BAC38,
    /* 0x11B */ (OpcodeFn)func_800B0EBC,
    /* 0x11C */ (OpcodeFn)func_800B43FC,
    /* 0x11D */ (OpcodeFn)func_800B65B0,
    /* 0x11E */ (OpcodeFn)func_800B2A70,
    /* 0x11F */ (OpcodeFn)func_800B4320,
    /* 0x120 */ (OpcodeFn)func_800BB140,
    /* 0x121 */ (OpcodeFn)func_800BB1F0,
    /* 0x122 */ (OpcodeFn)func_800BB2A4,
    /* 0x123 */ (OpcodeFn)func_800BB3D8,
    /* 0x124 */ (OpcodeFn)func_800BB510,
    /* 0x125 */ (OpcodeFn)func_800BAC18,
    /* 0x126 */ (OpcodeFn)func_800B1034,
    /* 0x127 */ (OpcodeFn)func_800B10F8,
    /* 0x128 */ (OpcodeFn)func_800BC58C,
    /* 0x129 */ (OpcodeFn)func_800B3350,
    /* 0x12A */ (OpcodeFn)func_800B8FA8,
    /* 0x12B */ (OpcodeFn)func_800B8F80,
    /* 0x12C */ (OpcodeFn)func_800B49E8,
    /* 0x12D */ (OpcodeFn)func_800B4A18,
    /* 0x12E */ (OpcodeFn)func_800B414C,
    /* 0x12F */ (OpcodeFn)func_800B0304,
    /* 0x130 */ (OpcodeFn)func_800B4A40,
    /* 0x131 */ (OpcodeFn)func_800B65D4,
    /* 0x132 */ (OpcodeFn)func_800B3F18,
    /* 0x133 */ (OpcodeFn)func_800B3F9C,
    /* 0x134 */ (OpcodeFn)func_800B4074,
    /* 0x135 */ (OpcodeFn)func_800B7718,
    /* 0x136 */ (OpcodeFn)func_800B7674,
    /* 0x137 */ (OpcodeFn)func_800B5188,
    /* 0x138 */ (OpcodeFn)func_800B63A4,
    /* 0x139 */ (OpcodeFn)func_800B6400,
    /* 0x13A */ (OpcodeFn)func_800B08CC,
    /* 0x13B */ (OpcodeFn)func_800B4A88,
    /* 0x13C */ (OpcodeFn)func_800B4DBC,
    /* 0x13D */ (OpcodeFn)func_800B74B0,
    /* 0x13E */ (OpcodeFn)func_800B7578,
    /* 0x13F */ (OpcodeFn)func_800B11BC,
    /* 0x140 */ (OpcodeFn)func_800B4EB0,
    /* 0x141 */ (OpcodeFn)func_800B4ED8,
    /* 0x142 */ (OpcodeFn)func_800B4FF8,
    /* 0x143 */ (OpcodeFn)func_800B0924,
    /* 0x144 */ (OpcodeFn)func_800B1730,
    /* 0x145 */ (OpcodeFn)func_800B6448,
    /* 0x146 */ (OpcodeFn)func_800B448C,
    /* 0x147 */ (OpcodeFn)func_800B1BB8,
    /* 0x148 */ (OpcodeFn)func_800B5318,
    /* 0x149 */ (OpcodeFn)func_800B5A30,
    /* 0x14A */ (OpcodeFn)func_800B4F80,
    /* 0x14B */ (OpcodeFn)func_800AFD20,
    /* 0x14C */ (OpcodeFn)func_800B5480,
    /* 0x14D */ (OpcodeFn)func_800BCE44,
    /* 0x14E */ (OpcodeFn)func_800BCECC,
    /* 0x14F */ (OpcodeFn)func_800BD1A4,
    /* 0x150 */ (OpcodeFn)func_800B3EA0,
    /* 0x151 */ (OpcodeFn)func_800B0E68,
    /* 0x152 */ (OpcodeFn)func_800B1ED4,
    /* 0x153 */ (OpcodeFn)func_800B1AA0,
    /* 0x154 */ (OpcodeFn)func_800AF5D4,
    /* 0x155 */ (OpcodeFn)func_800AF5C4,
    /* 0x156 */ (OpcodeFn)func_800B1B10,
    /* 0x157 */ (OpcodeFn)func_800B4DDC,
    /* 0x158 */ (OpcodeFn)func_800B4DFC,
    /* 0x159 */ (OpcodeFn)func_800B4E60,
    /* 0x15A */ (OpcodeFn)func_800B7640,
    /* 0x15B */ (OpcodeFn)func_800B2158,
    /* 0x15C */ (OpcodeFn)func_800B9488,
    /* 0x15D */ (OpcodeFn)func_800B94C0,
    /* 0x15E */ (OpcodeFn)func_800AF364,
    /* 0x15F */ (OpcodeFn)func_800AF3B4,
    /* 0x160 */ (OpcodeFn)func_800B62E8,
    /* 0x161 */ (OpcodeFn)func_800B6328,
    /* 0x162 */ (OpcodeFn)func_800B1DF4,
    /* 0x163 */ (OpcodeFn)func_800B51E0,
    /* 0x164 */ (OpcodeFn)func_800B5248,
    /* 0x165 */ (OpcodeFn)func_800B52B0,
    /* 0x166 */ (OpcodeFn)func_800B6364,
    /* 0x167 */ (OpcodeFn)func_800B6210,
    /* 0x168 */ (OpcodeFn)func_800B4D7C,
    /* 0x169 */ (OpcodeFn)func_800B0784,
    /* 0x16A */ (OpcodeFn)func_800B0818,
    /* 0x16B */ (OpcodeFn)func_800B085C,
    /* 0x16C */ (OpcodeFn)func_800B4D0C,
    /* 0x16D */ (OpcodeFn)func_800B9030,
    /* 0x16E */ (OpcodeFn)func_800B9078,
    /* 0x16F */ (OpcodeFn)func_800B90C0,
    /* 0x170 */ (OpcodeFn)func_800B536C,
    /* 0x171 */ (OpcodeFn)func_800B53D8,
    /* 0x172 */ (OpcodeFn)func_800B542C,
    /* 0x173 */ (OpcodeFn)func_800B505C,
    /* 0x174 */ (OpcodeFn)func_800B06D0,
    /* 0x175 */ (OpcodeFn)func_800B0638,
    /* 0x176 */ (OpcodeFn)func_800B16B0,
    /* 0x177 */ (OpcodeFn)func_800BD024,
    /* 0x178 */ (OpcodeFn)func_800BD1F4,
    /* 0x179 */ (OpcodeFn)func_800BD2AC,
    /* 0x17A */ (OpcodeFn)func_800BD318,
    /* 0x17B */ (OpcodeFn)func_800B9000,
    /* 0x17C */ (OpcodeFn)func_800B5134,
    /* 0x17D */ (OpcodeFn)func_800BA120,
    /* 0x17E */ (OpcodeFn)func_800BA1D0,
    /* 0x17F */ (OpcodeFn)func_800BA280,
    /* 0x180 */ (OpcodeFn)func_800BA330,
    /* 0x181 */ (OpcodeFn)func_800B1F04,
    /* 0x182 */ (OpcodeFn)func_800BAAFC,
    /* 0x183 */ (OpcodeFn)func_800BABFC,
    /* 0x184 */ (OpcodeFn)func_800B273C,
    /* 0x185 */ (OpcodeFn)func_800B2790,
    /* 0x186 */ (OpcodeFn)func_800B85C8,
    /* 0x187 */ (OpcodeFn)func_800B629C,
};
