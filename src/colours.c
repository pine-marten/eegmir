//
//      Predefined colours in 0xRRGGBB form, terminated by -999.  Put
//      any fixed colours required in here, and they will
//      automatically be converted to pixel values in colour[] when
//      SDL starts up.
//

int colour_data[]= {
   0x000000,    // 0 - black (0x80: white on black)
   0xFFFFFF,    // 1 - white
   0xCC0000,    // 2 - red (0x82: yellow on red) (required by console.c)
   0xFFFF00,    // 3 - yellow
   0xCC0000,    // 4 - red (0x84: white on red, for paging bar)
   0xFFFFFF,    // 5 - white
   0x000000,    // 6 - black (0x86: paging text)
   0xFFFFCC,    // 7 - off-white
   0x000099,    // 8 - blue (0x88: paging times)
   0x00CCFF,    // 9 - cyan
   0xFFFF55,    // 10 - temp background (0x8a: filled in by code when needed)
   0xFFFFFF,    // 11 - temp foreground
   0x005500,    // 12 - dark green (0x8c: settings text)
   0xCCFFCC,    // 13 - whitish
   0x660000,    // 14 - dark red (0x8e: settings code)
   0xFFFFFF,    // 15 - white
   0x0055AA,	// 16 - dark blue (0x90: settings value)
   0xFFFFFF,	// 17 - white
   0xCC0000,	// 18 - medium red (0x92: settings code highlighted)
   0xCCFFCC,	// 19 - white
   0x0000CC,	// 20 - blue (timing display background)
   0xFFFF00,	// 21 - yellow (timing display foreground)
   0xCC0000,	// 22 - red (timing display error background)
   0x000000,	// 23 - 
   0x000000,	// 24 - black (0x98: console warning messages)
   0xFF3333,	// 25 - red
   -999
};

// END //


