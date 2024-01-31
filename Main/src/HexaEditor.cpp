#include "stdafx.h"
using namespace Yahel;

	#define INI_HEXAEDITOR	_T("HexaEdit")

	static void InformationWithCheckableShowNoMore(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_HEXAEDITOR, messageId );
	}







	CHexaEditor::CYahelStreamFile::CYahelStreamFile()
		// ctor
		: nReferences(1) , position(0) , dataTotalLength(0) {
	}

	CHexaEditor::CYahelStreamFile::CYahelStreamFile(const CYahelStreamFile &r)
		// copy ctor
		: CFile() , nReferences(1) , position(r.position) , dataTotalLength(r.dataTotalLength) {
		ASSERT( r.m_hFile==CFile::hFileNull ); // must never be a filesystem instance!
	}

	CHexaEditor::CYahelStreamFile::~CYahelStreamFile(){
		// dtor
	}

	// IUnknown methods
	ULONG CHexaEditor::CYahelStreamFile::AddRef(){
		return ::InterlockedIncrement(&nReferences);
	}

	ULONG CHexaEditor::CYahelStreamFile::Release(){
		const auto n=::InterlockedDecrement(&nReferences);
		if (!n) delete this;
		return n;
	}

	HRESULT CHexaEditor::CYahelStreamFile::QueryInterface(REFIID riid,PVOID *ppvObject){
		static constexpr QITAB qit[]={
			QITABENT( CYahelStreamFile, IStream ),
			{ 0 },
		};
		return ::QISearch( this, qit, riid, ppvObject );
	}

	// IStream methods
	HRESULT CHexaEditor::CYahelStreamFile::Seek(LARGE_INTEGER dlibMove,DWORD dwOrigin,ULARGE_INTEGER *plibNewPosition){
		if (dwOrigin>STREAM_SEEK_END)
			return E_INVALIDARG;
		const TPosition seeked=static_cast<CFile *>(this)->Seek( dlibMove.QuadPart, dwOrigin );
		if (plibNewPosition)
			plibNewPosition->QuadPart=seeked;
		return S_OK;
	}

	HRESULT CHexaEditor::CYahelStreamFile::SetSize(ULARGE_INTEGER libNewSize){
		SetLength( libNewSize.QuadPart );
		return S_OK;
	}

	HRESULT CHexaEditor::CYahelStreamFile::CopyTo(IStream *pstm,ULARGE_INTEGER cb,ULARGE_INTEGER *pcbRead,ULARGE_INTEGER *pcbWritten){
		return E_NOTIMPL;
	}

	HRESULT CHexaEditor::CYahelStreamFile::Commit(DWORD grfCommitFlags){
		return E_NOTIMPL;
	}

	HRESULT CHexaEditor::CYahelStreamFile::Revert(){
		return E_NOTIMPL;
	}

	HRESULT CHexaEditor::CYahelStreamFile::LockRegion(ULARGE_INTEGER libOffset,ULARGE_INTEGER cb,DWORD dwLockType){
		return E_NOTIMPL;
	}

	HRESULT CHexaEditor::CYahelStreamFile::UnlockRegion(ULARGE_INTEGER libOffset,ULARGE_INTEGER cb,DWORD dwLockType){
		return E_NOTIMPL;
	}

	HRESULT CHexaEditor::CYahelStreamFile::Stat(STATSTG *pstatstg,DWORD grfStatFlag){
		if (!pstatstg)
			return E_INVALIDARG;
		::ZeroMemory( pstatstg, sizeof(*pstatstg) );
		pstatstg->type=STGTY_STREAM;
		pstatstg->cbSize.QuadPart=GetLength();
		return S_OK;
	}

	HRESULT CHexaEditor::CYahelStreamFile::Read(PVOID target,ULONG nCount,PULONG pcbRead){
		::SetLastError(ERROR_SUCCESS);
		const TPosition nBytesRead=static_cast<CFile *>(this)->Read( target, nCount );
		const auto err=::GetLastError();
		if (pcbRead)
			*pcbRead=nBytesRead;
		//if (nCount!=0 ^ nBytesRead!=0)
			//return E_FAIL;
		return err==ERROR_SUCCESS ? S_OK : S_FALSE; // successfully read healthy or erroneous data?
	}

	HRESULT CHexaEditor::CYahelStreamFile::Write(LPCVOID data,ULONG dataLength,PULONG pcbWritten){
		const auto orgPos=GetPosition();
		static_cast<CFile *>(this)->Write( data, dataLength );
		const auto nBytesWritten=GetPosition()-orgPos;
		if (pcbWritten)
			*pcbWritten=nBytesWritten;
		return	nBytesWritten ? S_OK : E_FAIL;
	}

	// CFile methods
#if _MFC_VER>=0x0A00
	ULONGLONG CHexaEditor::CYahelStreamFile::GetLength() const{
#else
	DWORD CHexaEditor::CYahelStreamFile::GetLength() const{
#endif
		// returns the File size
		return dataTotalLength;
	}

#if _MFC_VER>=0x0A00
	void CHexaEditor::CYahelStreamFile::SetLength(ULONGLONG dwNewLen){
#else
	void CHexaEditor::CYahelStreamFile::SetLength(DWORD dwNewLen){
#endif
		// overrides the reported FileSize
		ASSERT( dwNewLen<=dataTotalLength ); // can only "shrink" the reported FileSize
		dataTotalLength=dwNewLen;
		if (position>dataTotalLength)
			position=dataTotalLength;
	}

#if _MFC_VER>=0x0A00
	ULONGLONG CHexaEditor::CYahelStreamFile::GetPosition() const{
#else
	DWORD CHexaEditor::CYahelStreamFile::GetPosition() const{
#endif
		// returns the actual Position in the Serializer
		return position;
	}

#if _MFC_VER>=0x0A00
	ULONGLONG CHexaEditor::CYahelStreamFile::Seek(LONGLONG lOff,UINT nFrom){
#else
	LONG CHexaEditor::CYahelStreamFile::Seek(LONG lOff,UINT nFrom){
#endif
		// sets the actual Position in the Serializer
		switch ((SeekPosition)nFrom){
			case SeekPosition::current:
				lOff+=position;
				//fallthrough
			case SeekPosition::begin:
				position=std::min<TPosition>( lOff, dataTotalLength );
				break;
			case SeekPosition::end:
				position=std::max<TPosition>( dataTotalLength-lOff, 0 );
				break;
			default:
				ASSERT(FALSE);
		}
		return position;
	}









	#ifdef UNICODE
		#define GetBaseClassName IInstance::GetBaseClassNameW
	#else
		#define GetBaseClassName IInstance::GetBaseClassNameA
	#endif

	CHexaEditor::CHexaEditor(PVOID param)
		// ctor
		: CCtrlView( GetBaseClassName(AfxGetInstanceHandle()), AFX_WS_DEFAULT_VIEW&~WS_BORDER )
		, font(FONT_COURIER_NEW,105,false,true)
		, instance( IInstance::Create(AfxGetInstanceHandle(),this,param,font) )
		, hDefaultAccelerators( instance->GetAcceleratorTable() )
		, contextMenu( instance->GetContextMenu() ) {
		instance.p->Release();
	}







	void CHexaEditor::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		Invalidate();
	}

	void CHexaEditor::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	BOOL CHexaEditor::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:{
				// update
				int flags=GetCustomCommandMenuFlags(nID);
				if (flags<0){ // unknown custom command?
					flags=instance->GetDefaultCommandMenuFlags(nID);
					if (flags<0) // unknown default command?
						break;
				}
				CCmdUI *const pCmdUi=(CCmdUI *)pExtra;
				pCmdUi->Enable( (flags&(MF_GRAYED|MF_DISABLED))==0 );
				pCmdUi->SetCheck( (flags&MF_CHECKED)!=0 );
				return TRUE;
			}
			case CN_COMMAND:
				// command
				if (!ProcessCustomCommand(nID)){ // unknown custom command?
					LRESULT res;
					if (!instance->ProcessMessage( WM_COMMAND, nID, 0, res )) // unknown default command?
						break;
				}
				return TRUE;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	LRESULT CHexaEditor::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				// window created
				const LRESULT result=__super::WindowProc( msg, wParam, lParam );
				instance->Attach( m_hWnd );
				return result;
			}
			case WM_COMMAND:
				// command processing
				if (LOWORD(wParam)==ID_YAHEL_EDIT_COPY){
					LRESULT result;
					CComPtr<IDataObject> odo;
					instance->ProcessMessage( msg, wParam, (LPARAM)&odo.p, result );
					if (odo) // new data put into clipboard?
						CDos::GetFocused()->image->dataInClipboard=odo;
					return result;
				}else
					break;
			default:{
				LRESULT result;
				if (instance->ProcessMessage( msg, wParam, lParam, result )) // fully processed?
					return result;
				break;
			}
		}
		return __super::WindowProc( msg, wParam, lParam );
	}







	bool CHexaEditor::QueryNewSearchParams(TSearchParams &outSp) const{
		return outSp.EditModalWithDefaultEnglishDialog( m_hWnd, font );
	}

	struct TSearchParamsEx sealed:public TSearchParams{
		const IInstance &yahel;
		TPosition found;
		
		TSearchParamsEx(const TSearchParams &sp,const IInstance &yahel)
			: TSearchParams(sp)
			, yahel(yahel)
			, found(Stream::GetErrorPosition()) {
		}
	};

	static UINT AFX_CDECL Search_thread(PVOID pCancelableAction){
		// thread to perform search of specified Pattern
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		TSearchParamsEx &spe=*(TSearchParamsEx *)pAction->GetParams();
		// - search for next match of the Pattern
		const Yahel::TPosition fEnd=Stream::GetLength(spe.yahel.GetCurrentStream()), fCaret=spe.yahel.GetCaretPosition();
		pAction->SetProgressTarget( fEnd+1 ); // "+1" = to not preliminary end the search thread
		if (spe.searchForward){
			spe.found=spe.yahel.FindNextOccurence(
				TPosInterval( fCaret, fEnd ),
				pAction->Cancelled
			);
			if (pAction->Cancelled)
				return pAction->TerminateWithError( ERROR_CANCELLED );
			if (spe.found!=Stream::GetErrorPosition())
				return pAction->TerminateWithSuccess();
			if (Utils::QuestionYesNo( _T("No match found yet.\nContinue from the beginning?"), MB_DEFBUTTON1 )){
				spe.found=spe.yahel.FindNextOccurence(
					TPosInterval( 0, fCaret ),
					pAction->Cancelled
				);
				if (spe.found!=Stream::GetErrorPosition())
					return pAction->TerminateWithSuccess();
			}
		}else
			ASSERT(FALSE);
		// - no match found
		return pAction->TerminateWithError( ERROR_NOT_FOUND );
	}

	TPosition CHexaEditor::ContinueSearching(const TSearchParams &sp) const{
		TSearchParamsEx spe(sp,*instance);
		return	CBackgroundActionCancelable(
					Search_thread,
					&spe,
					THREAD_PRIORITY_BELOW_NORMAL
				).Perform()==ERROR_SUCCESS
				? spe.found
				: Stream::GetErrorPosition();
	}







	bool CHexaEditor::QueryAddressToGoTo(TGoToParams &outGtp) const{
		// navigating to an address input by the user
		return outGtp.EditModalWithDefaultEnglishDialog(*this);
	}







	bool CHexaEditor::QueryByteToResetSelectionWith(TResetSelectionParams &outRsp) const{
		// . defining the Dialog
		class CResetDialog sealed:public Utils::CRideDialog{
			BOOL OnInitDialog() override{
				TCHAR buf[80];
				::wsprintf( buf+GetDlgItemText(ID_DIRECTORY,buf,ARRAYSIZE(buf)), _T(" (0x%02X)"), directoryDefaultByte );
				SetDlgItemText( ID_DIRECTORY, buf );
				::wsprintf( buf+GetDlgItemText(ID_DATA,buf,ARRAYSIZE(buf)), _T(" (0x%02X)"), dataDefaultByte );
				SetDlgItemText( ID_DATA, buf );
				return __super::OnInitDialog();
			}
			void DoDataExchange(CDataExchange *pDX) override{
				DDX_Radio( pDX, ID_DIRECTORY, iRadioSel );
				if (!pDX->m_bSaveAndValidate || iRadioSel==3)
					DDX_Text( pDX, ID_NUMBER, value );
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				if (msg==WM_COMMAND)
					EnableDlgItem( ID_NUMBER, IsDlgItemChecked(ID_NUMBER2) );
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			const BYTE directoryDefaultByte, dataDefaultByte;
			int iRadioSel;
			BYTE value;

			CResetDialog(const CWnd *pParentWnd)
				: Utils::CRideDialog( IDR_HEXAEDITOR_RESETSELECTION, pParentWnd )
				, directoryDefaultByte( CDos::GetFocused()->properties->directoryFillerByte )
				, dataDefaultByte( CDos::GetFocused()->properties->sectorFillerByte )
				, iRadioSel(3) , value(0) {
			}
		} d(this);
		// . showing the Dialog and processing its result
		if (d.DoModal()==IDOK)
			switch (d.iRadioSel){
				case 0:
					outRsp.type=TResetSelectionParams::Byte, outRsp.byteValue=d.directoryDefaultByte;
					return true;
				case 1:
					outRsp.type=TResetSelectionParams::Byte, outRsp.byteValue=d.dataDefaultByte;
					return true;
				case 2:
					outRsp.type=TResetSelectionParams::GaussianNoise;
					return true;
				case 3:
					outRsp.type=TResetSelectionParams::Byte, outRsp.byteValue=d.value;
					return true;
				default:
					ASSERT(FALSE);
					return false;
			}
		return false;
	}







	bool CHexaEditor::ProcessCustomCommand(UINT cmd){
		// custom command processing
		return false; // no custom commands yet
	}

	int CHexaEditor::GetCustomCommandMenuFlags(WORD cmd) const{
		// custom command GUI update
		return -1; // unknown custom command
	}

	static bool DoPromptSingleTypeFileName(PWCHAR lpszFileNameBuffer,WORD bufferCapacity,LPCWSTR singleFilter,DWORD flags){
		const CString fileName=Utils::DoPromptSingleTypeFileName(
			lpszFileNameBuffer&&*lpszFileNameBuffer ? (LPCTSTR)Utils::ToStringT(lpszFileNameBuffer) : nullptr,
			singleFilter&&*singleFilter ? (LPCTSTR)Utils::ToStringT(singleFilter) : nullptr,
			flags
		);
		if (fileName.IsEmpty())
			return false;
		#ifdef UNICODE
			::lstrcpyn( lpszFileNameBuffer, fileName, bufferCapacity );			
		#else
			::MultiByteToWideChar( CP_ACP,0, fileName,-1, lpszFileNameBuffer,bufferCapacity );
		#endif
		return true;
	}

	bool CHexaEditor::ShowOpenFileDialog(LPCWSTR singleFilter,DWORD ofnFlags,PWCHAR lpszFileNameBuffer,WORD bufferCapacity) const{
		return DoPromptSingleTypeFileName( lpszFileNameBuffer, bufferCapacity, singleFilter, ofnFlags|OFN_FILEMUSTEXIST );
	}

	bool CHexaEditor::ShowSaveFileDialog(LPCWSTR singleFilter,DWORD ofnFlags,PWCHAR lpszFileNameBuffer,WORD bufferCapacity) const{
		return DoPromptSingleTypeFileName( lpszFileNameBuffer, bufferCapacity, singleFilter, ofnFlags );
	}

	void CHexaEditor::ShowInformation(TMsg id,UINT errorCode) const{
		#ifdef UNICODE
			const LPCWSTR msg=GetDefaultEnglishMessage(id);
		#else
			char msg[800];
			::WideCharToMultiByte( CP_ACP, 0, GetDefaultEnglishMessage(id),-1, msg,ARRAYSIZE(msg), nullptr,nullptr );
		#endif
		if (errorCode!=ERROR_SUCCESS)
			Utils::FatalError( msg, errorCode );
		else
			switch (id){
				case MSG_PADDED_TO_MINIMUM_SIZE:
					return InformationWithCheckableShowNoMore( msg, _T("msgpad") );
				default:
					return Utils::Information(msg);
			}
	}

	bool CHexaEditor::ShowQuestionYesNo(TMsg id,UINT defaultButton) const{
		#ifdef UNICODE
			const LPCWSTR msg=GetDefaultEnglishMessage(id);
		#else
			char msg[800];
			::WideCharToMultiByte( CP_ACP, 0, GetDefaultEnglishMessage(id),-1, msg,ARRAYSIZE(msg), nullptr,nullptr );
		#endif
		return Utils::QuestionYesNo( msg, defaultButton );
	}







	BOOL CHexaEditor::PreTranslateMessage(PMSG pMsg){
		// pre-processing the Message
		if (::GetFocus()==m_hWnd)
			if (::TranslateAccelerator( m_hWnd, hDefaultAccelerators, pMsg ))
				return TRUE;
		return __super::PreTranslateMessage(pMsg);
	}
