#pragma once

int getDeviceCodeFromStringHD(char* deviceCode);

/**
 * @brief These are stolen from KindleTool
 * 
 */
typedef enum
{
	Kindle1                       = 0x01,
	Kindle2US                     = 0x02,
	Kindle2International          = 0x03,
	KindleDXUS                    = 0x04,
	KindleDXInternational         = 0x05,
	KindleDXGraphite              = 0x09,
	Kindle3WiFi                   = 0x08,
	Kindle3WiFi3G                 = 0x06,
	Kindle3WiFi3GEurope           = 0x0A,
	Kindle4NonTouch               = 0x0E,    // Kindle 4 with a silver bezel, released fall 2011
	Kindle5TouchWiFi3G            = 0x0F,
	Kindle5TouchWiFi              = 0x11,
	Kindle5TouchWiFi3GEurope      = 0x10,
	Kindle5TouchUnknown           = 0x12,
	Kindle4NonTouchBlack          = 0x23,    // Kindle 4 with a black bezel, released fall 2012
	KindlePaperWhiteWiFi          = 0x24,    // Kindle PaperWhite (black bezel), released fall 2012 on FW 5.2.0
	KindlePaperWhiteWiFi3G        = 0x1B,
	KindlePaperWhiteWiFi3GCanada  = 0x1C,
	KindlePaperWhiteWiFi3GEurope  = 0x1D,
	KindlePaperWhiteWiFi3GJapan   = 0x1F,
	KindlePaperWhiteWiFi3GBrazil  = 0x20,
	KindlePaperWhite2WiFi         = 0xD4,    // Kindle PaperWhite 2 (black bezel), released fall 2013 on FW 5.4.0
	KindlePaperWhite2WiFiJapan    = 0x5A,
	KindlePaperWhite2WiFi3G       = 0xD5,
	KindlePaperWhite2WiFi3GCanada = 0xD6,
	KindlePaperWhite2WiFi3GEurope = 0xD7,
	KindlePaperWhite2WiFi3GRussia = 0xD8,
	KindlePaperWhite2WiFi3GJapan  = 0xF2,
	KindlePaperWhite2WiFi4GBInternational = 0x17,
	KindlePaperWhite2WiFi3G4GBEurope      = 0x60,
	KindlePaperWhite2Unknown_0xF4         = 0xF4,
	KindlePaperWhite2Unknown_0xF9         = 0xF9,
	KindlePaperWhite2WiFi3G4GB            = 0x62,
	KindlePaperWhite2WiFi3G4GBBrazil      = 0x61,
	KindlePaperWhite2WiFi3G4GBCanada      = 0x5F,
	KindleBasic                           = 0xC6,    // Kindle Basic (Pearl, Touch), released fall 2014 on FW 5.6.0
	KindleVoyageWiFi                      = 0x13,    // Kindle Voyage, released fall 2014 on FW 5.5.0
	ValidKindleUnknown_0x16               = 0x16,
	ValidKindleUnknown_0x21               = 0x21,
	KindleVoyageWiFi3G                    = 0x54,
	KindleVoyageWiFi3GJapan               = 0x2A,
	KindleVoyageWiFi3G_0x4F               = 0x4F,    // CA?
	KindleVoyageWiFi3GMexico              = 0x52,
	KindleVoyageWiFi3GEurope              = 0x53,
	ValidKindleUnknown_0x07               = 0x07,
	ValidKindleUnknown_0x0B               = 0x0B,
	ValidKindleUnknown_0x0C               = 0x0C,
	ValidKindleUnknown_0x0D               = 0x0D,
	ValidKindleUnknown_0x99               = 0x99,
	KindleBasicKiwi                       = 0xDD,
	/* KindlePaperWhite3 = 0x90, */    // Kindle PaperWhite 3, released summer 2015 on FW 5.6.1 (NOTE: This is a bogus ID, the proper one is now found at chars 4 to 6 of the S/N)
	KindlePaperWhite3WiFi                 = 0x201,    // 0G1
	KindlePaperWhite3WiFi3G               = 0x202,    // 0G2
	KindlePaperWhite3WiFi3GMexico         = 0x204,    // 0G4  NOTE: Might be better flagged as "Southern America"?
	KindlePaperWhite3WiFi3GEurope         = 0x205,    // 0G5
	KindlePaperWhite3WiFi3GCanada         = 0x206,    // 0G6
	KindlePaperWhite3WiFi3GJapan          = 0x207,    // 0G7
	// Kindle PaperWhite 3, White, appeared w/ FW 5.7.3.1, released summer 2016 on FW 5.7.x?
	KindlePaperWhite3WhiteWiFi            = 0x26B,            // 0KB
	KindlePaperWhite3WhiteWiFi3GJapan     = 0x26C,            // 0KC
	KindlePW3WhiteUnknown_0KD             = 0x26D,            // 0KD?
	KindlePaperWhite3WhiteWiFi3GInternational    = 0x26E,     // 0KE
	KindlePaperWhite3WhiteWiFi3GInternationalBis = 0x26F,     // 0KF
	KindlePW3WhiteUnknown_0KG                    = 0x270,     // 0KG?
	KindlePaperWhite3BlackWiFi32GBJapan          = 0x293,     // 0LK
	KindlePaperWhite3WhiteWiFi32GBJapan          = 0x294,     // 0LL
	KindlePW3Unknown_TTT                         = 0x6F7B,    // TTT?
	// Kindle Oasis, released late spring 2016 on FW 5.7.1.1
	KindleOasisWiFi                              = 0x20C,    // 0GC
	KindleOasisWiFi3G                            = 0x20D,    // 0GD
	KindleOasisWiFi3GInternational               = 0x219,    // 0GR
	KindleOasisUnknown_0GS                       = 0x21A,    // 0GS?
	KindleOasisWiFi3GChina                       = 0x21B,    // 0GT
	KindleOasisWiFi3GEurope                      = 0x21C,    // 0GU
	// Kindle Basic 2, released summer 2016 on FW 5.8.0
	KindleBasic2Unknown_0DU       = 0x1BC,    // 0DU??  FIXME: A good ID to check the sanity of my base32 tweaks...
	KindleBasic2                  = 0x269,    // 0K9 (Black)
	KindleBasic2White             = 0x26A,    // 0KA (White)
	// Kindle Oasis 2, released winter 2017 on FW 5.9.0.6
	KindleOasis2Unknown_0LM       = 0x295,    // 0LM?
	KindleOasis2Unknown_0LN       = 0x296,    // 0LN?
	KindleOasis2Unknown_0LP       = 0x297,    // 0LP?
	KindleOasis2Unknown_0LQ       = 0x298,    // 0LQ?
	KindleOasis2WiFi32GBChampagne = 0x2E1,    // 0P1
	KindleOasis2Unknown_0P2       = 0x2E2,    // 0P2?
	KindleOasis2Unknown_0P6 = 0x2E6,    // 0P6 (FIXME: Seen in the wild, WiFi+4G, 32GB, Graphite, not enough info)
	KindleOasis2Unknown_0P7 = 0x2E7,    // 0P7?
	KindleOasis2WiFi8GB     = 0x2E8,    // 0P8
	KindleOasis2WiFi3G32GB  = 0x341,    // 0S1
	KindleOasis2WiFi3G32GBEurope      = 0x342,    // 0S2
	KindleOasis2Unknown_0S3           = 0x343,    // 0S3?
	KindleOasis2Unknown_0S4           = 0x344,    // 0S4?
	KindleOasis2Unknown_0S7           = 0x347,    // 0S7?
	KindleOasis2WiFi32GB              = 0x34A,    // 0SA
	// Kindle PaperWhite 4, released November 7 2018 on FW 5.10.0.1/5.10.0.2
	KindlePaperWhite4WiFi8GB          = 0x2F7,    // 0PP
	KindlePaperWhite4WiFi4G32GB       = 0x361,    // 0T1
	KindlePaperWhite4WiFi4G32GBEurope = 0x362,    // 0T2
	KindlePaperWhite4WiFi4G32GBJapan  = 0x363,    // 0T3
	KindlePaperWhite4Unknown_0T4      = 0x364,    // 0T4?
	KindlePaperWhite4Unknown_0T5      = 0x365,    // 0T5?
	KindlePaperWhite4WiFi32GB         = 0x366,    // 0T6
	KindlePaperWhite4Unknown_0T7      = 0x367,    // 0T7?
	KindlePaperWhite4Unknown_0TJ      = 0x372,    // 0TJ?
	KindlePaperWhite4Unknown_0TK      = 0x373,    // 0TK?
	KindlePaperWhite4Unknown_0TL      = 0x374,    // 0TL?
	KindlePaperWhite4Unknown_0TM      = 0x375,    // 0TM?
	KindlePaperWhite4Unknown_0TN      = 0x376,    // 0TN?
	KindlePaperWhite4WiFi8GBIndia     = 0x402,    // 102 NOTE: Appeared in 5.10.1.3...
	KindlePaperWhite4WiFi32GBIndia    = 0x403,    // 103
	KindlePaperWhite4WiFi32GBBlue     = 0x4D8,    // 16Q (Twilight Blue, ??) NOTE: Appeared in 5.11.2...
	KindlePaperWhite4WiFi32GBPlum     = 0x4D9,    // 16R
	KindlePaperWhite4WiFi32GBSage     = 0x4DA,    // 16S
	KindlePaperWhite4WiFi8GBBlue      = 0x4DB,    // 16T (Twilight Blue, DE)
	KindlePaperWhite4WiFi8GBPlum      = 0x4DC,    // 16U (Plum. New batch of colors released summer 2020, on 5.12.3)
	KindlePaperWhite4WiFi8GBSage      = 0x4DD,    // 16V (Sage. Ditto)
	KindlePW4Unknown_0PL              = 0x2F4,    // 0PL?
	// Kindle Basic 3, released April 10 2019 on FW 5.1x.y
	KindleBasic3                      = 0x414,    // 10L
	KindleBasic3White8GB              = 0x3CF,    // 0WF (White, WiFi, DE. 4GB -> 8GB)
	KindleBasic3Unknown_0WG           = 0x3D0,    // 0WG?
	KindleBasic3White                 = 0x3D1,    // 0WH
	KindleBasic3Unknown_0WJ           = 0x3D2,    // 0WJ?
	KindleBasic3KidsEdition = 0x3AB,    // 0VB NOTE: Ships on a custom OTA-only FW branch. May be a special snowflake.
	// Kindle Oasis 3, released July 24 2019 on FW 5.12.0
	KindleOasis3WiFi32GBChampagne     = 0x434,    // 11L (Champagne, US)
	KindleOasis3WiFi4G32GBJapan       = 0x3D8,    // 0WQ (Graphite, JP)
	KindleOasis3WiFi4G32GBIndia       = 0x3D7,    // 0WP (Graphite, IN)
	KindleOasis3WiFi4G32GB            = 0x3D6,    // 0WN (Graphite, US)
	KindleOasis3WiFi32GB              = 0x3D5,    // 0WM (Graphite, DE)
	KindleOasis3WiFi8GB               = 0x3D4,    // 0WL (Graphite, DE)
	// Kindle PaperWhite 5, released October 27 2021 on FW 5.14.0
	KindlePaperWhite5SignatureEdition = 0x690,    // 1LG (Black, 32GB, US)
	KindlePaperWhite5Unknown_1Q0      = 0x700,    // 1Q0?
	KindlePaperWhite5                 = 0x6FF,    // 1PX (Black & White, 8GB, UK, FR, IT)
	KindlePaperWhite5Unknown_1VD      = 0x7AD,    // 1VD?
	KindlePaperWhite5SE_219           = 0x829,    // 219 (SE, 32GB, Denim, US)
	KindlePaperWhite5_21A             = 0x82A,    // 21A
	KindlePaperWhite5SE_2BH           = 0x971,    // 2BH NOTE: Appeared in 5.14.2... (SE)
	KindlePaperWhite5Unknown_2BJ      = 0x972,    // 2BJ?
	KindlePaperWhite5_2DK             = 0x9B3,    // 2DK NOTE: Appeared in 5.14.3... (Black, Kids or not, US)
	// Kindle Basic 4, released October 12 2022 on FW 5.15.0
	KindleBasic4Unknown_22D           = 0x84D,    // 22D?
	KindleBasic4Unknown_25T           = 0x8BB,    // 25T?
	KindleBasic4Unknown_23A           = 0x86A,    // 23A?
	KindleBasic4_2AQ                  = 0x958,    // 2AQ (Refurb seen in the wild)
	KindleBasic4_2AP                  = 0x957,    // 2AP (Seen in the wild, possibly EU-ish)
	KindleBasic4Unknown_1XH           = 0x7F1,    // 1XH?
	KindleBasic4Unknown_22C           = 0x84C,    // 22C?
	// Kindle Scribe, released December 2022 on FW 5.16.0
	KindleScribeUnknown_27J           = 0x8F2,    // 27J?
	KindleScribeUnknown_2BL           = 0x974,    // 2BL?
	KindleScribeUnknown_263           = 0x8C3,    // 263?
	KindleScribe16GB_227              = 0x847,    // 227 (JP, 16GB, Premium Pen)
	KindleScribeUnknown_2BM           = 0x975,    // 2BM?
	KindleScribe_23L                  = 0x874,    // 23L
	KindleScribe64GB_23M              = 0x875,    // 23M (US, 64GB, Premium Pen)
	KindleScribeUnknown_270           = 0x8E0,    // 270?
	// Kindle Basic 5, released October 2024 on FW 5.17.x
	KindleBasic5Unknown_3L5           = 0xE85,     // 3L5?
	KindleBasic5Unknown_3L6           = 0xE86,     // 3L6?
	KindleBasic5Unknown_3L4           = 0xE84,     // 3L4?
	KindleBasic5Unknown_3L3           = 0xE83,     // 3L3?
	KindleBasic5Unknown_A89           = 0x2909,    // A89?
	KindleBasic5Unknown_3L2           = 0xE82,     // 3L2?
	KindleBasic5Unknown_3KM           = 0xE75,     // 3KM
	// Kindle PaperWhite 6, released October 2024 on FW 5.17.x
	KindlePaperWhite6Unknown_349      = 0xC89,    // 349?
	KindlePaperWhite6Unknown_346      = 0xC86,    // 346?
	KindlePaperWhite6Unknown_33X      = 0xC7F,    // 33X
	KindlePaperWhite6Unknown_33W      = 0xC7E,    // 33W?
	KindlePaperWhite6Unknown_3HA      = 0xE2A,    // 3HA?
	KindlePaperWhite6Unknown_3H5      = 0xE25,    // 3H5?
	KindlePaperWhite6Unknown_3H3      = 0xE23,    // 3H3?
	KindlePaperWhite6Unknown_3H8      = 0xE28,    // 3H8?
	KindlePaperWhite6Unknown_3J5      = 0xE45,    // 3J5?
	KindlePaperWhite6Unknown_3JS      = 0xE5A,    // 3JS?
	// Kindle Scribe 2, released October 2024 on FW 5.17.x
	KindleScribe2Unknown_3V0		  = 0xFA0,     // 3V0?
	KindleScribe2Unknown_3V1		  = 0xFA1,     // 3V1?
	KindleScribe2Unknown_3X5		  = 0xFE5,     // 3X5?
	KindleScribe2Unknown_3UV		  = 0xF9D,     // 3UV?
	KindleScribe2Unknown_3X4		  = 0xFE4,     // 3X4?
	KindleScribe2Unknown_3X3		  = 0xFE3,     // 3X3?
	KindleScribe2Unknown_41E		  = 0x102E,    // 41E?
	KindleScribe2Unknown_41D		  = 0x102D,    // 41D?
	// Kindle ColorSoft, released October 2024 on FW 5.18.0
	KindleColorSoftUnknown_3H9        = 0xE29,     // 3H9?
	KindleColorSoftUnknown_3H4        = 0xE24,     // 3H4?
	KindleColorSoftUnknown_3HB        = 0xE2B,     // 3HB?
	KindleColorSoftUnknown_3H6        = 0xE26,     // 3H6?
	KindleColorSoftUnknown_3H2        = 0xE22,     // 3H2?
	KindleColorSoftUnknown_34X        = 0xC9F,     // 34X?
	KindleColorSoftUnknown_3H7        = 0xE27,     // 3H7
	KindleColorSoftUnknown_3JT        = 0xE5B,     // 3JT?
	KindleColorSoftUnknown_3J6        = 0xE46,     // 3J6?
	KindleColorSoftUnknown_456        = 0x10A6,    // 456?
	KindleColorSoftUnknown_455        = 0x10A5,    // 456?
	KindleColorSoftUnknown_4EP        = 0x11D7,    // 456?
	KindleUnknown                     = 0x00
} Device;