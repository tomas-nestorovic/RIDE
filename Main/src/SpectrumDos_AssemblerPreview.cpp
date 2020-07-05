#include "stdafx.h"

	CSpectrumBase::CAssemblerPreview *CSpectrumBase::CAssemblerPreview::pSingleInstance;

	#define INI_PREVIEW	_T("ZxZ80")

	#define PREVIEW_WIDTH_DEFAULT	750
	#define PREVIEW_HEIGHT_DEFAULT	300

	#define INI_FEATURES		_T("feats")
	#define INI_NUMBER_FORMAT	_T("numfmt")

	CSpectrumBase::CAssemblerPreview::CAssemblerPreview(const CFileManagerView &rFileManager)
		// ctor
		// - base
		: CFilePreview(	&contentView, INI_PREVIEW, rFileManager, PREVIEW_WIDTH_DEFAULT, PREVIEW_HEIGHT_DEFAULT, IDR_SPECTRUM_PREVIEW_ASSEMBLER )
		, contentView(_T(""))
		, numberFormat( (TNumberFormat)app.GetProfileInt(INI_PREVIEW,INI_NUMBER_FORMAT,TNumberFormat::HexaHashtag) ) {
		// - initialization
		features.info=-1; // by default, show all columns and turn on all other features ...
		features.capitalSyntax=0; // ... except for the CapitalSyntax
		features.info=app.GetProfileInt( INI_PREVIEW, INI_FEATURES, features.info );
		pSingleInstance=this;
		// - creating the TemporaryFile to store HTML-formatted content in
		::GetTempPath( sizeof(tmpFileName)/sizeof(TCHAR), tmpFileName );
		::GetTempFileName( tmpFileName, nullptr, FALSE, tmpFileName );
		// - creating the ContentView
		contentView.Create( nullptr, nullptr, WS_CHILD|WS_VISIBLE, rectDefault, this, AFX_IDW_PANE_FIRST, nullptr );
		contentView.OnInitialUpdate();
		// - showing the first File
		__showNextFile__();
	}

	CSpectrumBase::CAssemblerPreview::~CAssemblerPreview(){
		// dtor
		// - saving the settings
		app.WriteProfileInt( INI_PREVIEW, INI_FEATURES, features.info );
		app.WriteProfileInt( INI_PREVIEW, INI_NUMBER_FORMAT, numberFormat );
		// - uninitialization
		::DeleteFile(tmpFileName);
		pSingleInstance=nullptr;
	}







	#define DOS		rFileManager.tab.dos
	#define IMAGE	DOS->image

	#define PREVIEW_LABEL	"Z80 assembler listing"

	#define Z80_INST_ADC	_T("adc")
	#define Z80_INST_ADD	_T("add")
	#define Z80_INST_AND	_T("and")
	#define Z80_INST_BIT	_T("bit")
	#define Z80_INST_CALL	_T("call")
	#define Z80_INST_CCF	_T("ccf")
	#define Z80_INST_CP		_T("cp")
	#define Z80_INST_CPD	_T("cpd")
	#define Z80_INST_CPDR	_T("cpdr")
	#define Z80_INST_CPI	_T("cpi")
	#define Z80_INST_CPIR	_T("cpir")
	#define Z80_INST_CPL	_T("cpl")
	#define Z80_INST_DAA	_T("daa")
	#define Z80_INST_DEC	_T("dec")
	#define Z80_INST_DI		_T("di")
	#define Z80_INST_DJNZ	_T("djnz")
	#define Z80_INST_EI		_T("ei")
	#define Z80_INST_EX		_T("ex")
	#define Z80_INST_EXX	_T("exx")
	#define Z80_INST_HALT	_T("halt")
	#define Z80_INST_IM		_T("im")
	#define Z80_INST_IN		_T("in")
	#define Z80_INST_INC	_T("inc")
	#define Z80_INST_IND	_T("ind")
	#define Z80_INST_INDR	_T("indr")
	#define Z80_INST_INI	_T("ini")
	#define Z80_INST_INIR	_T("inir")
	#define Z80_INST_JP		_T("jp")
	#define Z80_INST_JR		_T("jr")
	#define Z80_INST_LD		_T("ld")
	#define Z80_INST_LDD	_T("ldd")
	#define Z80_INST_LDDR	_T("lddr")
	#define Z80_INST_LDI	_T("ldi")
	#define Z80_INST_LDIR	_T("ldir")
	#define Z80_INST_NEG	_T("neg")
	#define Z80_INST_NOP	_T("nop")
	#define Z80_INST_OR		_T("or")
	#define Z80_INST_OUT	_T("out")
	#define Z80_INST_OUTD	_T("outd")
	#define Z80_INST_OTDR	_T("otdr")
	#define Z80_INST_OUTI	_T("outi")
	#define Z80_INST_OTIR	_T("otir")
	#define Z80_INST_POP	_T("pop")
	#define Z80_INST_PUSH	_T("push")
	#define Z80_INST_RES	_T("res")
	#define Z80_INST_RET	_T("ret")
	#define Z80_INST_RETI	_T("reti")
	#define Z80_INST_RETN	_T("retn")
	#define Z80_INST_RL		_T("rl")
	#define Z80_INST_RLA	_T("rla")
	#define Z80_INST_RLC	_T("rlc")
	#define Z80_INST_RLCA	_T("rlca")
	#define Z80_INST_RLD	_T("rld")
	#define Z80_INST_RR		_T("rr")
	#define Z80_INST_RRA	_T("rra")
	#define Z80_INST_RRC	_T("rrc")
	#define Z80_INST_RRCA	_T("rrca")
	#define Z80_INST_RRD	_T("rrd")
	#define Z80_INST_RST	_T("rst")
	#define Z80_INST_SBC	_T("sbc")
	#define Z80_INST_SCF	_T("scf")
	#define Z80_INST_SET	_T("set")
	#define Z80_INST_SLA	_T("sla")
	#define Z80_INST_SRA	_T("sra")
	#define Z80_INST_SLL	_T("sll")
	#define Z80_INST_SRL	_T("srl")
	#define Z80_INST_SUB	_T("sub")
	#define Z80_INST_XOR	_T("xor")

	void CSpectrumBase::CAssemblerPreview::ParseZ80BinaryFileAndGenerateHtmlFormattedContent(CFile &fIn,CFile &f) const{
		// generates HTML-formatted Z80 instruction listing of the input File into a temporary file
		Utils::WriteToFile( f, _T("<h2>Z80 Assembler Listing</h2>") );
		if (fIn.GetPosition()>=fIn.GetLength())
			Utils::WriteToFile( f, _T("None") );
		else{
			bool error=false; // assumption (no parsing error)
			if (features.colorSyntax)
				Utils::WriteToFile( f, _T("<style>span.num{color:#D00} span.pair{color:#00C} span.inst{color:#333;font-weight:bold}</style>") );
			Utils::WriteToFile( f, _T("<table cellpadding=3 cellspacing=0 style=\"font-family:'Courier New'\">") );
			struct TOp sealed{
				const TNumberFormat numberFormat;
				CFile &fIn;
				DWORD orgAddress;
				BYTE length;
				BYTE machineCode[8];
				BYTE code;
				BYTE index; // for IX/IY offsets
				BYTE b;
				WORD w;
				TCHAR strB[32],strW[32];

				TOp(TNumberFormat numberFormat,CFile &fIn,DWORD orgAddress)
					: numberFormat(numberFormat) , fIn(fIn)
					, orgAddress(orgAddress) , length(0) {
				}

				bool Read(BYTE &b){
					if (fIn.Read(&b,sizeof(BYTE))==sizeof(BYTE)){
						machineCode[length++]=b;
						return true;
					}else
						return false;
				}

				bool ReadNextIndex(){
					return Read(index);
				}

				void Parse(int i,PTCHAR buf) const{
					switch (numberFormat){
						case TNumberFormat::HexaHashtag:
							::wsprintf( buf, _T("<span class=num>#%X</span>"), i );
							break;
						case TNumberFormat::Hexa0x:
							::wsprintf( buf, _T("<span class=num>0x%X</span>"), i );
							break;
						case TNumberFormat::HexaH:
							::wsprintf( buf, _T("<span class=num>%Xh</span>"), i );
							break;
						case TNumberFormat::Decadic:
							::wsprintf( buf, _T("<span class=num>%d</span>"), i );
							break;
					}
				}

				bool ParseNextByte(){
					if (!Read(b))
						return false;
					Parse( b, strB );
					return true;
				}

				bool ParseNextWord(){
					if (fIn.Read(&w,sizeof(w))!=sizeof(w))
						return false;
					*(PWORD)(machineCode+length)=w;
					length+=sizeof(WORD);
					Parse( w, strW );
					return true;
				}

				void Reset(){
					orgAddress+=length;
					length=0;
				}
			} op( numberFormat, fIn, 0 ); // Z80 instruction with parameters
			static const TCHAR Registers[]=_T("bcdehlfa");
			static const TCHAR RegisterPairsStd[]=_T("bcdehlsp");
			static const TCHAR OperandsStd[]=_T("   b   c   d   e   h   l %sj   a");
			static const TCHAR OperandsIxIy[]=_T("   b   c   d   e %sh %sl %si   a");
			static const TCHAR Conditions[]=_T("nz znc cpope p m");
			LPCTSTR registerPairs=RegisterPairsStd, operands=OperandsStd;
			do{
				if (error=!op.Read(op.code)) // error
					break;
				LPCTSTR inst; // Z80 instruction ...
				TCHAR argSyntaxBuf[64]; *argSyntaxBuf='\0'; LPCTSTR argSyntax=argSyntaxBuf; // ... and its argument syntax (by default none)
				switch (op.code){
					case 0x00: // nop
						inst=Z80_INST_NOP;
						break;
					case 0x01: // ld bc,NN
					case 0x11: // ld de,NN
					case 0x21: // ld {hl,ix,iy},NN
					case 0x31: // ld sp,NN
						if (error=!op.ParseNextWord()) // error
							break;
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>,%s"), registerPairs+(op.code>>3), op.strW );
						break;
					case 0x02: // ld (bc),a
					case 0x12: // ld (de),a
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("(<span class=pair>%.2s</span>),a"), RegisterPairsStd+(op.code>>3) );
						break;
					case 0x03: // inc bc
					case 0x13: // inc de
					case 0x23: // inc {hl,ix,iy}
					case 0x33: // inc sp
						inst=Z80_INST_INC;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>"), registerPairs+(op.code>>3) );
						break;
					case 0x04: // inc b
					case 0x0C: // inc c
					case 0x14: // inc d
					case 0x1C: // inc e
					case 0x24: // inc h
					case 0x2C: // inc l
					case 0x34: // inc {(hl),(ix+N),(iy+N)}
					case 0x3C: // inc a
						inst=Z80_INST_INC;
						::wsprintf( argSyntaxBuf, _T("%.4s"), operands+((op.code-0x04)>>1) );
						if (operands!=OperandsStd)
							error=!op.ReadNextIndex(); // error
						break;
					case 0x05: // dec b
					case 0x0D: // dec c
					case 0x15: // dec d
					case 0x1D: // dec e
					case 0x25: // dec h
					case 0x2D: // dec l
					case 0x35: // dec {(hl),(ix+N),(iy+N)}
					case 0x3D: // dec a
						inst=Z80_INST_DEC;
						::wsprintf( argSyntaxBuf, _T("%.4s"), operands+((op.code-0x05)>>1) );
						if (operands!=OperandsStd)
							error=!op.ReadNextIndex(); // error
						break;
					case 0x06: // ld b,N
					case 0x0E: // ld c,N
					case 0x16: // ld d,N
					case 0x1E: // ld e,N
					case 0x26: // ld h,N
					case 0x2E: // ld l,N
					case 0x36: // ld {(hl),(ix+N),(iy+N)},N
					case 0x3E: // ld a,N
						if (operands!=OperandsStd)
							if (error=!op.ReadNextIndex()) // error
								break;
						if (error=!op.ParseNextByte()) // error
							break;
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("%.4s,%s"), operands+((op.code-0x06)>>1), op.strB );
						break;
					case 0x07: // rlca
						inst=Z80_INST_RLCA;
						break;
					case 0x0F: // rrca
						inst=Z80_INST_RRCA;
						break;
					case 0x17: // rla
						inst=Z80_INST_RLA;
						break;
					case 0x1F: // rra
						inst=Z80_INST_RRA;
						break;
					case 0x08: // ex af,af'
						inst=Z80_INST_EX;
						argSyntax=_T("af,af'");
						break;
					case 0x09: // add {hl,ix,iy},bc
					case 0x19: // add {hl,ix,iy},de
					case 0x29: // add {hl,ix,iy},{hl,ix,iy}
					case 0x39: // add {hl,ix,iy},sp
						inst=Z80_INST_ADD;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>,<span class=pair>%.2s</span>"), registerPairs+4, registerPairs+((op.code&0xf0)>>3) );
						break;
					case 0x0A: // ld a,(bc)
					case 0x1A: // ld a,(de)
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("a,(<span class=pair>%.2s</span>)"), registerPairs+((op.code&0xf0)>>3) );
						break;
					case 0x0B: // dec bc
					case 0x1B: // dec de
					case 0x2B: // dec {hl,ix,iy}
					case 0x3B: // dec sp
						inst=Z80_INST_DEC;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>"), registerPairs+((op.code&0xf0)>>3) );
						break;
					case 0x10: // djnz NN
						if (error=!op.ParseNextByte()) // error
							break;
						inst=Z80_INST_DJNZ;
						argSyntax=op.strB;
						break;
					case 0x20: // jr nz,N
					case 0x28: // jr  z,N
					case 0x30: // jr nc,N
					case 0x38: // jr  c,N
						::wsprintf( argSyntaxBuf, _T("%.2s,"), Conditions+((op.code-0x20)>>2) );
						//fallthrough
					case 0x18: // jr N
						if (error=!op.ParseNextByte()) // error
							break;
						inst=Z80_INST_JR;
						::lstrcat( argSyntaxBuf, op.strB );
						break;
					case 0x22: // ld (NN),{hl,ix,iy}
						if (error=!op.ParseNextWord()) // error
							break;
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("(%s),<span class=pair>%.2s</span>"), op.strW, registerPairs+4 );
						break;
					case 0x27: // daa
						inst=Z80_INST_DAA;
						break;
					case 0x2A: // ld {hl,ix,iy},(NN)
						if (error=!op.ParseNextWord()) // error
							break;
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>,(%s)"), registerPairs+4, op.strW );
						break;
					case 0x2F: // cpl
						inst=Z80_INST_CPL;
						break;
					case 0x32: // ld (NN),a
						if (error=!op.ParseNextWord()) // error
							break;
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("(%s),a"), op.strW );
						break;
					case 0x37: // scf
						inst=Z80_INST_SCF;
						break;
					case 0x3A: // ld a,(NN)
						if (error=!op.ParseNextWord()) // error
							break;
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("a,(%s)"), op.strW );
						break;
					case 0x3F: // ccf
						inst=Z80_INST_CCF;
						break;
					case 0x76: // halt
						inst=Z80_INST_HALT;
						break;
					case 0xC0: // ret nz
					case 0xC8: // ret  z
					case 0xD0: // ret nc
					case 0xD8: // ret  c
					case 0xE0: // ret po
					case 0xE8: // ret pe
					case 0xF0: // ret  p
					case 0xF8: // ret  m
						::wsprintf( argSyntaxBuf, _T("%.2s"), Conditions+((op.code-0xC0)>>2) );
						//fallthrough
					case 0xC9: // ret
						inst=Z80_INST_RET;
						break;
					case 0xC1: // pop bc
					case 0xD1: // pop de
					case 0xE1: // pop {hl,ix,iy}
						inst=Z80_INST_POP;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>"), registerPairs+((op.code-0xC1)>>3) );
						break;
					case 0xF1: // pop af
						inst=Z80_INST_POP;
						argSyntax=_T("af");
						break;
					case 0xC2: // jp nz,NN
					case 0xCA: // jp  z,NN
					case 0xD2: // jp nc,NN
					case 0xDA: // jp  c,NN
					case 0xE2: // jp po,NN
					case 0xEA: // jp pe,NN
					case 0xF2: // jp  p,NN
					case 0xFA: // jp  m,NN
						::wsprintf( argSyntaxBuf, _T("%.2s,"), Conditions+((op.code-0xC2)>>2) );
						//fallthrough
					case 0xC3: // jp NN
						if (error=!op.ParseNextWord()) // error
							break;
						inst=Z80_INST_JP;
						::lstrcat( argSyntaxBuf, op.strW );
						break;
					case 0xC4: // call nz,NN
					case 0xCC: // call  z,NN
					case 0xD4: // call nc,NN
					case 0xDC: // call  c,NN
					case 0xE4: // call po,NN
					case 0xEC: // call pe,NN
					case 0xF4: // call  p,NN
					case 0xFC: // call  m,NN
						::wsprintf( argSyntaxBuf, _T("%.2s,"), Conditions+((op.code-0xC4)>>2) );
						//fallthrough
					case 0xCD: // call NN
						if (error=!op.ParseNextWord()) // error
							break;
						inst=Z80_INST_CALL;
						::lstrcat( argSyntaxBuf, op.strW );
						break;
					case 0xC5: // push bc
					case 0xD5: // push de
					case 0xE5: // push {hl,ix,iy}
						inst=Z80_INST_PUSH;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>"), registerPairs+((op.code-0xC5)>>3) );
						break;
					case 0xF5: // push af
						inst=Z80_INST_PUSH;
						argSyntax=_T("af");
						break;
					case 0xC6: // add a,N
					case 0xCE: // adc a,N
					case 0xDE:{// sbc a,N
						if (error=!op.ParseNextByte()) // error
							break;
						static const LPCTSTR Instructions[]={ Z80_INST_ADD, Z80_INST_ADC, nullptr, Z80_INST_SBC };
						inst=Instructions[(op.code-0xC6)>>3];
						::wsprintf( argSyntaxBuf, _T("a,%s"), op.strB );
						break;
					}
					case 0xC7: // rst 0
					case 0xCF: // rst 8
					case 0xD7: // rst 16
					case 0xDF: // rst 24
					case 0xE7: // rst 32
					case 0xEF: // rst 40
					case 0xF7: // rst 48
					case 0xFF: // rst 54
						inst=Z80_INST_RST;
						op.Parse( op.code-0xC7, argSyntaxBuf );
						break;
					case 0xCB:
						if (operands!=OperandsStd)
							if (error=!op.ReadNextIndex()) // error
								break;
						if (error=!op.Read( op.code ))
							break;
						if (op.code<0x40){
							static const LPCTSTR Instructions[]={ Z80_INST_RLC, Z80_INST_RRC, Z80_INST_RL, Z80_INST_RR, Z80_INST_SLA, Z80_INST_SRA, Z80_INST_SLL, Z80_INST_SRL };
							inst=Instructions[op.code>>3];
							::wsprintf( argSyntaxBuf, _T("%.4s"), operands+((op.code&7)<<2) );
						}else if (op.code<0x80){
							inst=Z80_INST_BIT;
							::wsprintf( argSyntaxBuf, _T("%d,%.4s"), (op.code-0x40)>>3, operands+((op.code&7)<<2) );
						}else if (op.code<0xC0){
							inst=Z80_INST_RES;
							::wsprintf( argSyntaxBuf, _T("%d,%.4s"), (op.code-0x80)>>3, operands+((op.code&7)<<2) );
						}else{
							inst=Z80_INST_SET;
							::wsprintf( argSyntaxBuf, _T("%d,%.4s"), (op.code-0xC0)>>3, operands+((op.code&7)<<2) );
						}
						break;
					case 0xD3: // out (N),a
						if (error=!op.ParseNextByte()) // error
							break;
						inst=Z80_INST_OUT;
						::wsprintf( argSyntaxBuf, _T("(%s),a"), op.strB );
						break;
					case 0xD6: // sub N
					case 0xE6: // and N
					case 0xEE: // xor N
					case 0xF6: // or N
					case 0xFE:{// cp N
						if (error=!op.ParseNextByte()) // error
							break;
						static const LPCTSTR Instructions[]={ Z80_INST_SUB, nullptr, Z80_INST_AND, Z80_INST_XOR, Z80_INST_OR, Z80_INST_CP };
						inst=Instructions[(op.code-0xD6)>>3];
						argSyntax=op.strB;
						break;
					}
					case 0xD9: // exx
						inst=Z80_INST_EXX;
						break;
					case 0xDB: // in a,(N)
						if (error=!op.ParseNextByte()) // error
							break;
						inst=Z80_INST_IN;
						::wsprintf( argSyntaxBuf, _T("a,(%s)"), op.strB );
						break;
					case 0xDD: // prefix for the IY register pair
						if (error=operands!=OperandsStd) // error (two consecutive prefixes are not allowed)
							break;
						registerPairs=_T("bcdeixsp"); // RegisterPairsIx
						operands=OperandsIxIy;
						continue;
					case 0xE3: // ex (sp),{hl,ix,iy}
						inst=Z80_INST_EX;
						::wsprintf( argSyntaxBuf, _T("(<span class=pair>sp</span>),<span class=pair>%.2s</span>"), registerPairs+4 );
						break;
					case 0xE9: // jp {(hl),(ix),(iy)}
						inst=Z80_INST_JP;
						::wsprintf( argSyntaxBuf, _T("(<span class=pair>%.2s</span>)"), registerPairs+4 );
						break;
					case 0xEB: // ex de,{hl,ix,iy}
						inst=Z80_INST_EX;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>de</span>,<span class=pair>%.2s</span>"), registerPairs+4 );
						break;
					case 0xED:
						if (error=!op.Read( op.code )) // error
							break;
						if (error=op.code<0x40) // error
							break;
						switch (op.code){
							case 0x40: // in b,(c)
							case 0x48: // in c,(c)
							case 0x50: // in d,(c)
							case 0x58: // in e,(c)
							case 0x60: // in h,(c)
							case 0x68: // in l,(c)
							case 0x70: // in f,(c)
							case 0x78: // in a,(c)
								inst=Z80_INST_IN;
								::wsprintf( argSyntaxBuf, _T("%c,(c)"), Registers[(op.code-0x40)>>3] );
								break;
							case 0x41: // out (c),b
							case 0x49: // out (c),c
							case 0x51: // out (c),d
							case 0x59: // out (c),e
							case 0x61: // out (c),h
							case 0x69: // out (c),l
							case 0x79: // out (c),a
								inst=Z80_INST_OUT;
								::wsprintf( argSyntaxBuf, _T("(c),%c"), Registers[(op.code-0x41)>>3] );
								break;
							case 0x71: // out (c),0 (unofficial)
								inst=Z80_INST_OUT;
								argSyntax=_T("(c),0");
								break;
							case 0x42: // sbc {hl,ix,iy},bc
							case 0x52: // sbc {hl,ix,iy},de
							case 0x62: // sbc {hl,ix,iy},{hl,ix,iy}
							case 0x72: // sbc {hl,ix,iy},sp
								inst=Z80_INST_SBC;
								::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>,<span class=pair>%.2s</span>"), registerPairs+4, registerPairs+((op.code-0x42)>>3) );
								break;
							case 0x43: // ld (NN),bc
							case 0x53: // ld (NN),de
							case 0x63: // ld (NN),{hl,ix,iy}
							case 0x73: // ld (NN),sp
								if (error=!op.ParseNextWord()) // error
									break;
								inst=Z80_INST_LD;
								::wsprintf( argSyntaxBuf, _T("(%s),<span class=pair>%.2s</span>"), op.strW, registerPairs+((op.code-0x43)>>3) );
								break;
							case 0x44: // neg
							case 0x4C: // neg (unofficial)
							case 0x54: // neg (unofficial)
							case 0x5C: // neg (unofficial)
							case 0x64: // neg (unofficial)
							case 0x6C: // neg (unofficial)
							case 0x74: // neg (unofficial)
							case 0x7C: // neg (unofficial)
								inst=Z80_INST_NEG;
								break;
							case 0x45: // retn
							case 0x55: // retn (unofficial)
							case 0x5D: // retn (unofficial)
							case 0x65: // retn (unofficial)
							case 0x6D: // retn (unofficial)
							case 0x75: // retn (unofficial)
							case 0x7D: // retn (unofficial)
								inst=Z80_INST_RETN;
								break;
							case 0x46: // im 0
							case 0x66: // im 0 (unofficial)
								inst=Z80_INST_IM;
								argSyntax=_T("0");
								break;
							case 0x56: // im 1
							case 0x76: // im 1 (unofficial)
								inst=Z80_INST_IM;
								argSyntax=_T("1");
								break;
							case 0x5E: // im 2
							case 0x7E: // im 2 (unofficial)
								inst=Z80_INST_IM;
								argSyntax=_T("2");
								break;
							case 0x47: // ld i,a
								inst=Z80_INST_LD;
								argSyntax=_T("i,a");
								break;
							case 0x57: // ld a,i
								inst=Z80_INST_LD;
								argSyntax=_T("a,i");
								break;
							case 0x4A: // adc {hl,ix,iy},bc
							case 0x5A: // adc {hl,ix,iy},de
							case 0x6A: // adc {hl,ix,iy},{hl,ix,iy}
							case 0x7A: // adc {hl,ix,iy},sp
								inst=Z80_INST_ADC;
								::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>,<span class=pair>%.2s</span>"), registerPairs+4, registerPairs+((op.code-0x4A)>>3) );
								break;
							case 0x4B: // ld bc,(NN)
							case 0x5B: // ld de,(NN)
							case 0x6B: // ld {hl,ix,iy},(NN)
							case 0x7B: // ld sp,(NN)
								if (error=!op.ParseNextWord()) // error
									break;
								inst=Z80_INST_LD;
								::wsprintf( argSyntaxBuf, _T("<span class=pair>%.2s</span>,(%s)"), registerPairs+((op.code-0x4B)>>3), op.strW );
								break;
							case 0x4D: // reti
								inst=Z80_INST_RETI;
								break;
							case 0x4F: // ld r,a
								inst=Z80_INST_LD;
								argSyntax=_T("r,a");
								break;
							case 0x5F: // ld a,r
								inst=Z80_INST_LD;
								argSyntax=_T("a,r");
								break;
							case 0x67: // rrd
								inst=Z80_INST_RRD;
								break;
							case 0x6F: // rld
								inst=Z80_INST_RLD;
								break;
							case 0x77: // nop (unofficial)
							case 0x7F: // nop (unofficial)
								inst=Z80_INST_NOP;
								break;
							case 0xA0: // ldi
								inst=Z80_INST_LDI;
								break;
							case 0xA1: // cpi
								inst=Z80_INST_CPI;
								break;
							case 0xA2: // ini
								inst=Z80_INST_INI;
								break;
							case 0xA3: // outi
								inst=Z80_INST_OUTI;
								break;
							case 0xA8: // ldd
								inst=Z80_INST_LDD;
								break;
							case 0xA9: // cpd
								inst=Z80_INST_CPD;
								break;
							case 0xAA: // ind
								inst=Z80_INST_IND;
								break;
							case 0xAB: // outd
								inst=Z80_INST_OUTD;
								break;
							case 0xB0: // ldir
								inst=Z80_INST_LDIR;
								break;
							case 0xB1: // cpir
								inst=Z80_INST_CPIR;
								break;
							case 0xB2: // inir
								inst=Z80_INST_INIR;
								break;
							case 0xB3: // otir
								inst=Z80_INST_OTIR;
								break;
							case 0xB8: // lddr
								inst=Z80_INST_LDDR;
								break;
							case 0xB9: // cpdr
								inst=Z80_INST_CPDR;
								break;
							case 0xBA: // indr
								inst=Z80_INST_INDR;
								break;
							case 0xBB: // otdr
								inst=Z80_INST_OTDR;
								break;
							default:
								error=true; // no other instructions prefixed with 0xED
								break;
						}
						break;
					case 0xF3: // di
						inst=Z80_INST_DI;
						break;
					case 0xF9: // ld sp,{hl,ix,iy}
						inst=Z80_INST_LD;
						::wsprintf( argSyntaxBuf, _T("<span class=pair>sp</span>,<span class=pair>%.2s</span>"), registerPairs+4 );
						break;
					case 0xFB: // ei
						inst=Z80_INST_EI;
						break;
					case 0xFD: // prefix for the IY register pair
						if (error=operands!=OperandsStd) // error (two consecutive prefixes are not allowed)
							break;
						registerPairs=_T("bcdeiysp"); // RegisterPairsIy
						operands=OperandsIxIy;
						continue;
					default:
						if (error=op.code<0x40) // just to be sure - op codes under 0x40 inclusive should be covered in Cases above
							break;
						if (op.code<0x80){
							// ld op,op
							inst=Z80_INST_LD;
							::wsprintf( argSyntaxBuf, _T("%.4s,%.4s"), operands+(((op.code-0x40)&~7)>>1), operands+((op.code&7)<<2) );
						}else if (op.code<0x90){
							static const LPCTSTR Instructions[]={ Z80_INST_ADD, Z80_INST_ADC };
							inst=Instructions[(op.code-0x80)>>3];
							::wsprintf( argSyntaxBuf, _T("a,%.4s"), operands+((op.code&7)<<2) );
						}else if (op.code<0x98){
							inst=Z80_INST_SUB;
							::wsprintf( argSyntaxBuf, _T("%.4s"), operands+((op.code&7)<<2) );
						}else if (op.code<0xA0){
							inst=Z80_INST_SBC;
							::wsprintf( argSyntaxBuf, _T("a,%.4s"), operands+((op.code&7)<<2) );
						}else if (op.code<0xC0){
							static const LPCTSTR Instructions[]={ Z80_INST_AND, Z80_INST_XOR, Z80_INST_OR, Z80_INST_CP };
							inst=Instructions[(op.code-0xA0)>>3];
							::wsprintf( argSyntaxBuf, _T("%.4s"), operands+((op.code&7)<<2) );
						}
						if (operands!=OperandsStd)
							error=!op.ReadNextIndex(); // error
						break;
				}
				if (error)
					break;
				Utils::WriteToFile( f, _T("<tr>") );
					if (features.address){
						Utils::WriteToFileFormatted( f, _T("<td align=right style=\"padding-right:40pt\">%X</td>"), op.orgAddress );
					}
					if (features.machineCode){
						Utils::WriteToFile( f, _T("<td style=\"padding-right:40pt\">") );
							for( BYTE i=0; i<op.length; i++ )
								Utils::WriteToFileFormatted( f, _T("%02X "), op.machineCode[i] );
						Utils::WriteToFile( f, _T("</td>") );
					}
					if (features.instruction){
						if (const LPCTSTR percent=_tcschr( argSyntax, '%' )){
							TCHAR format[sizeof(argSyntaxBuf)/sizeof(TCHAR)], param[16];
							TCHAR c=' ';
							std::swap( ::lstrcpy(format,argSyntax)[percent-argSyntax+2], c );
							switch (c){
								case 'j':
									::lstrcpy( param, _T("(<span class=pair>hl</span>)") );
									break;
								case 'h':
									::wsprintf( param, _T("<i>High</i>(<span class=pair>%.2s</span>)"), registerPairs+4 );
									break;
								case 'l':
									::wsprintf( param, _T("<i>Low</i>(<span class=pair>%.2s</span>)"), registerPairs+4 );
									break;
								case 'i':
									op.Parse( op.index, op.strB );
									::wsprintf( param, _T("(<span class=pair>%.2s</span>+%s)"), registerPairs+4, op.strB );
									break;
							}
							::wsprintf( argSyntaxBuf, format, param );
							argSyntax=argSyntaxBuf;
						}
						TCHAR instBuf[8];
						if (features.capitalSyntax){
							inst=::CharUpper( ::lstrcpy(instBuf,inst) );
							argSyntax=::CharUpper( ::lstrcpy(argSyntaxBuf,argSyntax) );
						}
						Utils::WriteToFileFormatted( f, _T("<td style=\"padding-right:16pt\"><span class=inst>%s</span></td><td>%s</td>"), inst, argSyntax );
					}
				Utils::WriteToFile( f, _T("</tr>") );
				registerPairs=RegisterPairsStd, operands=OperandsStd; // reset to standard HL register pair
				op.Reset();
			} while (fIn.GetPosition()<fIn.GetLength());
			Utils::WriteToFile( f, _T("</table>") );
			if (error)
				Utils::WriteToFile( f, _T("<p style=\"color:red\">Error in machine code parsing!</p>") );
		}
	}

	void CSpectrumBase::CAssemblerPreview::RefreshPreview(){
		// refreshes the Preview (e.g. when switched to another File)
		if (const PCFile file=pdt->entry){
			// . creating the File reader
			CFileReaderWriter frw(DOS,pdt->entry);
			BYTE a,z;
			const DWORD fileOfficialSize=DOS->GetFileSize( pdt->entry, &a, &z, CDos::TGetFileSizeOptions::OfficialDataLength );
			frw.SetLength( a+fileOfficialSize ); // ignoring appended custom data (e.g. as in TR-DOS)
			frw.Seek( a, CFile::begin ); // ignoring prepended custom data (e.g. as in GDOS)
			// . generating the HTML-formatted content
			CFile f( tmpFileName, CFile::modeWrite|CFile::modeCreate );
				Utils::WriteToFileFormatted( f, _T("<html><body style=\"background-color:#%06x\">"), *(PCINT)&Colors[7] );
					ParseZ80BinaryFileAndGenerateHtmlFormattedContent( frw, f );
				Utils::WriteToFile( f, _T("</body></html>") );
			f.Close();
			// . opening the HTML-formatted content
			contentView.Navigate2(tmpFileName);
			// . updating the window caption
			CString caption;
			caption.Format( PREVIEW_LABEL " (%s)", (LPCTSTR)DOS->GetFilePresentationNameAndExt(file) );
			SetWindowText(caption);
		}else
			SetWindowText(PREVIEW_LABEL);
		SetWindowPos( nullptr, 0,0, 0,0, SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED );
	}

	BOOL CSpectrumBase::CAssemblerPreview::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				switch (nID){
					case ID_ADDRESS:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(features.address);
						return TRUE;
					case ID_DATA:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(features.machineCode);
						return TRUE;
					case ID_INFORMATION:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(features.instruction);
						return TRUE;
					case ID_COLOR:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(features.colorSyntax);
						return TRUE;
					case ID_ALIGN:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(features.capitalSyntax);
						return TRUE;
					case ID_DEFAULT1:
					case ID_DEFAULT2:
					case ID_DEFAULT3:
					case ID_DEFAULT4:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck( nID-ID_DEFAULT1 == numberFormat );
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_ADDRESS:
						features.address=!features.address;
						RefreshPreview();
						return TRUE;
					case ID_DATA:
						features.machineCode=!features.machineCode;
						RefreshPreview();
						return TRUE;
					case ID_INFORMATION:
						features.instruction=!features.instruction;
						RefreshPreview();
						return TRUE;
					case ID_COLOR:
						features.colorSyntax=!features.colorSyntax;
						RefreshPreview();
						return TRUE;
					case ID_ALIGN:
						features.capitalSyntax=!features.capitalSyntax;
						RefreshPreview();
						return TRUE;
					case ID_DEFAULT1:
					case ID_DEFAULT2:
					case ID_DEFAULT3:
					case ID_DEFAULT4:
						numberFormat=(TNumberFormat)(nID-ID_DEFAULT1);
						RefreshPreview();
						return TRUE;
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}
