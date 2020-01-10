#ifndef PROPGRID_API_H
#define PROPGRID_API_H

	#ifdef _DEBUG
		#define PROPGRID_DECLSPEC
	#elif PROPGRID_EXPORT
		#define PROPGRID_DECLSPEC __declspec(dllexport)
	#else
		#define PROPGRID_DECLSPEC __declspec(dllimport)
	#endif

	#define PROPGRID_CELL_MARGIN_LEFT	3
	#define PROPGRID_CELL_MARGIN_TOP	1

	namespace PropGrid{
		typedef short TSize;
		typedef PVOID PCustomParam,PValue;
		typedef LPCVOID PCEditor,PCValue;

		typedef void (WINAPI *TDrawValueHandler)(PCustomParam,PCValue,TSize,PDRAWITEMSTRUCT);
		typedef bool (WINAPI *TOnEllipsisButtonClicked)(PCustomParam,PValue,TSize);
		typedef void (WINAPI *TOnValueChanged)(PCustomParam);

		namespace String{
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,PValue,TSize);
			typedef bool (WINAPI *TOnValueConfirmedA)(PCustomParam,LPCSTR,TSize);
			typedef bool (WINAPI *TOnValueConfirmedW)(PCustomParam,LPCWSTR,TSize);

			PROPGRID_DECLSPEC PCEditor DefineFixedLengthEditorA(
				TSize nCharsMax,
				TOnValueConfirmedA onValueConfirmed=nullptr,
				char paddingChar='\0',
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineFixedLengthEditorW(
				TSize nCharsMax,
				TOnValueConfirmedW onValueConfirmed=nullptr,
				WCHAR paddingChar='\0',
				TOnValueChanged onValueChanged=nullptr
			);

			PROPGRID_DECLSPEC PCEditor DefineDynamicLengthEditorA(
				TOnValueConfirmedA onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineDynamicLengthEditorW(
				TOnValueConfirmedW onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);

			PROPGRID_DECLSPEC PCEditor DefineFileNameEditorA(
				TSize nCharsMax,
				TOnValueConfirmedA onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineFileNameEditorW(
				TSize nCharsMax,
				TOnValueConfirmedW onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);
		}

		namespace Boolean{
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,bool);

			PROPGRID_DECLSPEC PCEditor DefineEditor(
				TSize nValueBytes,
				TOnValueConfirmed onValueConfirmed,
				TOnValueChanged onValueChanged=nullptr,
				DWORD reservedValue=0,
				bool reservedForTrue=false
			);
			PROPGRID_DECLSPEC PCEditor DefineByteEditor(
				TOnValueConfirmed onValueConfirmed,
				TOnValueChanged onValueChanged=nullptr,
				BYTE reservedValue=0,
				bool reservedForTrue=false
			);
		}

		namespace Integer{
			typedef const struct PROPGRID_DECLSPEC TUpDownLimits sealed{
				static const TUpDownLimits PositiveByte; // 1..255
				static const TUpDownLimits PositiveWord; // 1..65535
				static const TUpDownLimits PositiveInteger; // 1..INT_MAX
				static const TUpDownLimits NonNegativeInteger; // 0..INT_MAX
				static const TUpDownLimits NegativeInteger; // INT_MIN..-1

				int iMin,iMax;
			} &RCUpDownLimits;
			enum TFeatures:BYTE{
				NONE		=0,
				HEXADECIMAL	=1,
				ALIGN_RIGHT	=2
			};
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,int);

			PROPGRID_DECLSPEC PCEditor DefineEditor(
				TSize nValueBytes,
				RCUpDownLimits rLimits,
				TOnValueConfirmed onValueConfirmed=nullptr,
				BYTE features=TFeatures::NONE,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineByteEditor(
				TOnValueConfirmed onValueConfirmed=nullptr,
				BYTE features=TFeatures::NONE,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefinePositiveByteEditor(
				TOnValueConfirmed onValueConfirmed=nullptr,
				BYTE features=TFeatures::NONE,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineWordEditor(
				TOnValueConfirmed onValueConfirmed=nullptr,
				BYTE features=TFeatures::NONE,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefinePositiveWordEditor(
				TOnValueConfirmed onValueConfirmed=nullptr,
				BYTE features=TFeatures::NONE,
				TOnValueChanged onValueChanged=nullptr
			);
		}

		namespace Enum{
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
			typedef LPCTSTR (WINAPI *TGetValueDesc)(PCustomParam,UValue,PTCHAR,TSize);
			typedef LPCSTR (WINAPI *TGetValueDescA)(PCustomParam,UValue,PCHAR,TSize);
			typedef LPCWSTR (WINAPI *TGetValueDescW)(PCustomParam,UValue,PWCHAR,TSize);
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,UValue);

			PROPGRID_DECLSPEC PCEditor DefineConstStringListEditorA(
				TSize nValueBytes,
				TGetValueList getValueList, // list of possible values
				TGetValueDescA getValueDesc, // value-to-string conversion
				TFreeValueList freeValueList, // can be null for PROPGRID_DECLSPEC lists of values
				TOnValueConfirmed onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineConstStringListEditorW(
				TSize nValueBytes,
				TGetValueList getValueList, // list of possible values
				TGetValueDescW getValueDesc, // value-to-string conversion
				TFreeValueList freeValueList, // can be null for PROPGRID_DECLSPEC lists of values
				TOnValueConfirmed onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineConstCustomListEditor(
				WORD height, // height of editor in pixels (0 = default property height)
				TSize nValueBytes,
				TGetValueList getValueList, // list of possible values
				TDrawValueHandler drawValue,
				TFreeValueList freeValueList, // can be null for PROPGRID_DECLSPEC lists of values
				TOnValueConfirmed onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);
		}

		namespace Hyperlink{
			typedef bool (WINAPI *TOnHyperlinkClicked)(PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName);
			typedef bool (WINAPI *TOnHyperlinkClickedA)(PCustomParam,int hyperlinkId,LPCSTR hyperlinkName);
			typedef bool (WINAPI *TOnHyperlinkClickedW)(PCustomParam,int hyperlinkId,LPCWSTR hyperlinkName);

			PROPGRID_DECLSPEC PCEditor DefineEditorA(
				TOnHyperlinkClickedA onHyperlinkClicked,
				TOnValueChanged onValueChanged=nullptr
			);
			PROPGRID_DECLSPEC PCEditor DefineEditorW(
				TOnHyperlinkClickedW onHyperlinkClicked,
				TOnValueChanged onValueChanged=nullptr
			);
		}

		namespace Custom{
			typedef HWND HParentWnd;
			typedef HWND (WINAPI *TCreateCustomMainEditor)(PValue,TSize,HParentWnd);
			typedef bool (WINAPI *TOnValueConfirmed)(PCustomParam,HWND,PValue);

			PROPGRID_DECLSPEC PCEditor DefineEditor(
				WORD height, // height of editor in pixels (0 = default property height)
				TSize nValueBytes,
				TDrawValueHandler drawValue,
				TCreateCustomMainEditor createCustomMainEditor, // Null <=> the editor doesn't feature a main control, otherwise the function should return a control well initialized to the current value (eventually featuring an attached UpDownControl)
				TOnEllipsisButtonClicked onEllipsisBtnClicked, // Null <=> the editor doesn't feature an ellipsis button, otherwise the function is a handler for the on-click event on the ellipsis button
				TOnValueConfirmed onValueConfirmed=nullptr,
				TOnValueChanged onValueChanged=nullptr
			);
		}

		PROPGRID_DECLSPEC LPCTSTR WINAPI GetWindowClass(HINSTANCE hInstance);
		PROPGRID_DECLSPEC HWND WINAPI Create(HINSTANCE hInstance,LPCTSTR windowName,UINT style,int x,int y,int width,int height,HWND hParent);
		PROPGRID_DECLSPEC HANDLE WINAPI AddPropertyW(HWND hPropGrid,HANDLE category,LPCWSTR name,PValue value,PCEditor editor,PCustomParam param=nullptr);
		PROPGRID_DECLSPEC HANDLE WINAPI AddPropertyA(HWND hPropGrid,HANDLE category,LPCSTR name,PValue value,PCEditor editor,PCustomParam param=nullptr);
		PROPGRID_DECLSPEC HANDLE WINAPI AddDisabledPropertyW(HWND hPropGrid,HANDLE category,LPCWSTR name,PValue value,PCEditor editor,PCustomParam param=nullptr);
		PROPGRID_DECLSPEC HANDLE WINAPI AddDisabledPropertyA(HWND hPropGrid,HANDLE category,LPCSTR name,PValue value,PCEditor editor,PCustomParam param=nullptr);
		PROPGRID_DECLSPEC HANDLE WINAPI AddCategoryW(HWND hPropGrid,HANDLE category,LPCWSTR name,bool initiallyExpanded=true);
		PROPGRID_DECLSPEC HANDLE WINAPI AddCategoryA(HWND hPropGrid,HANDLE category,LPCSTR name,bool initiallyExpanded=true);
		PROPGRID_DECLSPEC HANDLE WINAPI EnableProperty(HWND hPropGrid,HANDLE propOrCat,bool enabled);
		PROPGRID_DECLSPEC void WINAPI RemoveProperty(HWND hPropGrid,HANDLE propOrCat);
		PROPGRID_DECLSPEC HWND WINAPI CreateUpDownControl(HWND hEdit,UINT style,bool bHexadecimal,Integer::RCUpDownLimits rLimits,int iCurrent);
		PROPGRID_DECLSPEC short WINAPI GetCurrentlySelectedProperty(HWND hPropGrid);
		PROPGRID_DECLSPEC short WINAPI SetCurrentlySelectedProperty(HWND hPropGrid,short iSelected);
		PROPGRID_DECLSPEC HWND WINAPI BeginEditValue(PValue value,PCustomParam param,PCEditor editor,RECT rcEditorRect,DWORD style,HWND hParent,HWND *pOutEllipsisBtn);
		PROPGRID_DECLSPEC bool WINAPI TryToAcceptCurrentValueAndCloseEditor();
		PROPGRID_DECLSPEC bool WINAPI IsValueBeingEdited();

		#ifdef UNICODE
			#define AddProperty AddPropertyW
			#define AddDisabledProperty AddDisabledPropertyW
			#define AddCategory AddCategoryW
		#else
			#define AddProperty AddPropertyA
			#define AddDisabledProperty AddDisabledPropertyA
			#define AddCategory AddCategoryA
		#endif
	}

#endif // PROPGRID_API_H
