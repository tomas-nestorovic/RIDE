#ifndef PROPGRID_API_H
#define PROPGRID_API_H

	#ifdef _DEBUG
		#define PROPGRID_DECLSPEC
	#elif PROPGRID_EXPORT
		#define PROPGRID_DECLSPEC __declspec(dllexport)
	#else
		#define PROPGRID_DECLSPEC __declspec(dllimport)
	#endif


	class PROPGRID_DECLSPEC CPropGridCtrl sealed{
	public:
		typedef short TValueSize,TBufferCapacity;
		typedef PVOID PCustomParam,PValue;
		typedef LPCVOID PCEditor,PCValue;

		typedef void (WINAPI *TDrawValueHandler)(PCustomParam,PCValue,TValueSize,PDRAWITEMSTRUCT);
		typedef bool (WINAPI *TOnEllipsisButtonClicked)(PCustomParam,PValue,TValueSize);
		typedef void (WINAPI *TOnValueChanged)(PCustomParam);

		struct PROPGRID_DECLSPEC TString sealed{
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,PValue,TValueSize);
			typedef bool (WINAPI *TOnValueConfirmedA)(PCustomParam,LPCSTR,TValueSize);
			typedef bool (WINAPI *TOnValueConfirmedW)(PCustomParam,LPCWSTR,TValueSize);

			static PCEditor DefineFixedLengthEditorA(TOnValueConfirmedA onValueConfirmed=NULL,char paddingChar='\0',TOnValueChanged onValueChanged=NULL);
			static PCEditor DefineFixedLengthEditorW(TOnValueConfirmedW onValueConfirmed=NULL,WCHAR paddingChar='\0',TOnValueChanged onValueChanged=NULL);

			static PCEditor DefineDynamicLengthEditorA(TOnValueConfirmedA onValueConfirmed=NULL,TOnValueChanged onValueChanged=NULL);
			static PCEditor DefineDynamicLengthEditorW(TOnValueConfirmedW onValueConfirmed=NULL,TOnValueChanged onValueChanged=NULL);

			static PCEditor DefineFileNameEditorA(TOnValueConfirmedA onValueConfirmed=NULL,TOnValueChanged onValueChanged=NULL);
			static PCEditor DefineFileNameEditorW(TOnValueConfirmedW onValueConfirmed=NULL,TOnValueChanged onValueChanged=NULL);
		};

		struct PROPGRID_DECLSPEC TBoolean sealed{
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,bool);

			static PCEditor DefineEditor(TOnValueConfirmed onValueConfirmed,TOnValueChanged onValueChanged=NULL,DWORD reservedValue=0,bool reservedForTrue=false);
		};

		struct PROPGRID_DECLSPEC TInteger sealed{
			typedef const struct TUpDownLimits sealed{
				int iMin,iMax;
			} &RCUpDownLimits;
			enum TFeatures:BYTE{
				NONE		=0,
				HEXADECIMAL	=1,
				ALIGN_RIGHT	=2
			};
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,int);

			static const TUpDownLimits PositiveByteLimits; // 1..255
			static const TUpDownLimits PositiveWordLimits; // 1..65535
			static const TUpDownLimits PositiveIntegerLimits; // 1..INT_MAX
			static const TUpDownLimits NonNegativeIntegerLimits; // 0..INT_MAX
			static const TUpDownLimits NegativeIntegerLimits; // INT_MIN..-1

			static PCEditor DefineEditor(RCUpDownLimits rLimits,TOnValueConfirmed onValueConfirmed=NULL,BYTE features=TFeatures::NONE,TOnValueChanged onValueChanged=NULL);
			static PCEditor DefineByteEditor(TOnValueConfirmed onValueConfirmed=NULL,BYTE features=TFeatures::NONE,TOnValueChanged onValueChanged=NULL);
			static PCEditor DefinePositiveByteEditor(TOnValueConfirmed onValueConfirmed=NULL,BYTE features=TFeatures::NONE,TOnValueChanged onValueChanged=NULL);
			static PCEditor DefineWordEditor(TOnValueConfirmed onValueConfirmed=NULL,BYTE features=TFeatures::NONE,TOnValueChanged onValueChanged=NULL);
			static PCEditor DefinePositiveWordEditor(TOnValueConfirmed onValueConfirmed=NULL,BYTE features=TFeatures::NONE,TOnValueChanged onValueChanged=NULL);
		};

		struct PROPGRID_DECLSPEC TEnum sealed{
			typedef WORD &RValueCount;
			union UValue{
				char charValue;
				short shortValue;
				int intValue;
				LONG_PTR longValue;
			};
			typedef LPCVOID PCValueList;
			typedef PCValueList (WINAPI *TGetValueList)(PCustomParam,RValueCount);
			typedef void (WINAPI *TFreeValueList)(PCustomParam,PCValueList);
			typedef LPCTSTR (WINAPI *TGetValueDesc)(PCustomParam,UValue,PTCHAR,TBufferCapacity);
			typedef LPCSTR (WINAPI *TGetValueDescA)(PCustomParam,UValue,PCHAR,TBufferCapacity);
			typedef LPCWSTR (WINAPI *TGetValueDescW)(PCustomParam,UValue,PWCHAR,TBufferCapacity);
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,UValue);

			static PCEditor DefineConstStringListEditorA(
								TGetValueList getValueList, // list of possible values
								TGetValueDescA getValueDesc, // value-to-string conversion
								TFreeValueList freeValueList, // can be null for static lists of values
								TOnValueConfirmed onValueConfirmed=NULL,
								TOnValueChanged onValueChanged=NULL
							);
			static PCEditor DefineConstStringListEditorW(
								TGetValueList getValueList, // list of possible values
								TGetValueDescW getValueDesc, // value-to-string conversion
								TFreeValueList freeValueList, // can be null for static lists of values
								TOnValueConfirmed onValueConfirmed=NULL,
								TOnValueChanged onValueChanged=NULL
							);
			//*
			static PCEditor DefineConstCustomListEditor(
								WORD height, // height of editor in pixels (0 = default property height)
								TGetValueList getValueList, // list of possible values
								TDrawValueHandler drawValue,
								TFreeValueList freeValueList, // can be null for static lists of values
								TOnValueConfirmed onValueConfirmed=NULL,
								TOnValueChanged onValueChanged=NULL
							);
			//*/
		};

		struct PROPGRID_DECLSPEC THyperlink sealed{
			typedef bool (WINAPI *TOnHyperlinkClicked)(PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName);
			typedef bool (WINAPI *TOnHyperlinkClickedA)(PCustomParam,int hyperlinkId,LPCSTR hyperlinkName);
			typedef bool (WINAPI *TOnHyperlinkClickedW)(PCustomParam,int hyperlinkId,LPCWSTR hyperlinkName);

			static PCEditor DefineEditorA(TOnHyperlinkClickedA onHyperlinkClicked,TOnValueChanged onValueChanged=NULL);
			static PCEditor DefineEditorW(TOnHyperlinkClickedW onHyperlinkClicked,TOnValueChanged onValueChanged=NULL);
		};

		struct PROPGRID_DECLSPEC TCustom sealed{
			typedef HWND HParentWnd;
			typedef HWND (WINAPI *TCreateCustomMainEditor)(PValue,TValueSize,HParentWnd);
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,HWND,PValue,TValueSize);

			static PCEditor DefineEditor(
								WORD height, // height of editor in pixels (0 = default property height)
								TDrawValueHandler drawValue,
								TCreateCustomMainEditor createCustomMainEditor, // Null <=> the editor doesn't feature a main control, otherwise the function should return a control well initialized to the current value (eventually featuring an attached UpDownControl)
								TOnEllipsisButtonClicked onEllipsisBtnClicked, // Null <=> the editor doesn't feature an ellipsis button, otherwise the function is a handler for the on-click event on the ellipsis button
								TOnValueConfirmed onValueConfirmed=NULL,
								TOnValueChanged onValueChanged=NULL
							);
		};

		static LPCTSTR WINAPI GetWindowClass(HINSTANCE hInstance);
		static HWND WINAPI Create(HINSTANCE hInstance,LPCTSTR windowName,UINT style,int x,int y,int width,int height,HWND hParent);
		static HANDLE WINAPI AddProperty(HWND hPropGrid,HANDLE category,LPCTSTR name,PValue value,TValueSize valueBytes,PCEditor editor,PCustomParam param=NULL);
		static HANDLE WINAPI AddCategory(HWND hPropGrid,HANDLE category,LPCTSTR name,bool initiallyExpanded=true);
		static HANDLE WINAPI EnableProperty(HWND hPropGrid,HANDLE propOrCat,bool enabled);
		static void WINAPI RemoveProperty(HWND hPropGrid,HANDLE propOrCat);
		static HWND WINAPI CreateUpDownControl(HWND hEdit,UINT style,bool bHexadecimal,TInteger::RCUpDownLimits rLimits,int iCurrent);
		static short WINAPI GetCurrentlySelectedProperty(HWND hPropGrid);
		static short WINAPI SetCurrentlySelectedProperty(HWND hPropGrid,short iSelected);
		static HWND WINAPI BeginEditValue(PValue value,TValueSize valueBytes,PCustomParam param,PCEditor editor,RECT rcEditorRect,DWORD style,HWND hParent,HWND *pOutEllipsisBtn);
		static bool WINAPI TryToAcceptCurrentValueAndCloseEditor();
		static bool WINAPI IsValueBeingEdited();
	};

#endif // PROPGRID_API_H
