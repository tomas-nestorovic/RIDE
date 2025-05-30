#include "stdafx.h"

	#define INI_RECOGNITION_ORDER	_T("recogn")

	#define INI_RECOGNITION_DOS_ID	_T("%08x")

	CDos::CRecognition::CRecognition()
		// ctor
		// - initialization
		: nDoses(0) {
		// - loading the Recognition Order from profile
		bool unknownDosAdded=false; // UnknownDos serves as the delimiter between recognized and ignored DOSes
		const CString s=app.GetProfileString(INI_GENERAL,INI_RECOGNITION_ORDER);
		LPCTSTR ps=s;
		for( TId dosId=0,nCharsRead=0; _stscanf(ps,INI_RECOGNITION_DOS_ID _T("%n"),&dosId,&nCharsRead)>0; ps+=nCharsRead )
			if (dosId!=CUnknownDos::Properties.id)
				for( POSITION pos=Known.GetHeadPosition(); pos; ){
					const PCProperties props=Known.GetNext(pos);
					if (props->id==dosId){
						order[++nDoses]=props; // indexing starts from 1
						break;
					}
				}
			else{
				order[++nDoses]=&CUnknownDos::Properties; // indexing starts from 1
				unknownDosAdded=true;
			}
		// - adding the Unknown DOS, just in case it wasn't added above (because the profile string didn't exist)
		if (!unknownDosAdded)
			order[++nDoses]=&CUnknownDos::Properties;
	}

	void CDos::CRecognition::SaveToProfile() const{
		// saves the Recognition Order to profile
		TCHAR buf[1024],*p=buf;
		for( BYTE i=1; i<=nDoses; p+=::wsprintf(p,INI_RECOGNITION_DOS_ID,order[i++]->id) ); // indexing starts from 1
		app.WriteProfileString( INI_GENERAL, INI_RECOGNITION_ORDER, buf );
	}

	BYTE CDos::CRecognition::AddDosByPriorityDescending(PCProperties props){
		// adds specified DOS to the earliest possible moment in recognition, respecting eventual previous user-defined ordering; returns the 1-based index at which it was added to
		BYTE i=GetOrderIndex(&CUnknownDos::Properties);
		while (i>1 && props->recognitionPriority>order[i-1]->recognitionPriority) // indexing starts from 1
			i--;
		::memmove(	&order[i+1],
					&order[i],
					sizeof(PCProperties)*(++nDoses-i)
				);
		order[i]=props;
		return i;
	}

	BYTE CDos::CRecognition::GetOrderIndex(PCProperties props) const{
		// returns 1-based Order index of the particular DOS (or 0 if DOS not found in the Order array)
		for( BYTE i=1; i<=nDoses; i++ ) // indexing starts from 1
			if (order[i]==props)
				return i;
		return 0;
	}

	BYTE CDos::CRecognition::GetFirstRecognizedDosPosition() const{
		// returns the position of the first DOS that participates in Image recognition
		return	order[1]->IsKnown(); // indexing starts from 1; returns 0 if Order empty
	}
	CDos::PCProperties CDos::CRecognition::GetNextRecognizedDos(BYTE &pos) const{
		// returns the Properties of the next DOS that participates in Image recognition
		const PCProperties result=order[pos++];
		if (!order[pos]->IsKnown())
			pos=0;
		return result;
	}

	struct TRecognitionParams sealed{
		const CDos::CRecognition &recognition;
		const PImage image;
		const PFormat pOutFormatBoot;
		BYTE pos;
		CDos::PCProperties props;

		TRecognitionParams(const CDos::CRecognition &recognition,PImage image,PFormat pOutFormatBoot)
			// ctor
			: recognition(recognition) , image(image) , pOutFormatBoot(pOutFormatBoot)
			, pos( recognition.GetFirstRecognizedDosPosition() )
			, props(nullptr) {
		}
	};

	UINT AFX_CDECL CDos::CRecognition::Thread(PVOID pCancelableAction){
		// thread to recognize an implemented DOS
		CBackgroundActionCancelable &bac=*(CBackgroundActionCancelable *)pCancelableAction;
		TRecognitionParams &rp=*(TRecognitionParams *)bac.GetParams();
		bac.SetProgressTarget( rp.recognition.GetOrderIndex(&CUnknownDos::Properties) ); // indexing starts from 1, hence N+1 is the return value!
		while (rp.pos){
			if (bac.Cancelled)
				return ERROR_CANCELLED;
			else
				switch (( rp.props=rp.recognition.GetNextRecognizedDos(rp.pos) )->fnRecognize(rp.image,rp.pOutFormatBoot)){
					case ERROR_SUCCESS:
						return bac.TerminateWithSuccess();
					case ERROR_CANCELLED:
						return bac.TerminateWithError(ERROR_CANCELLED);
				}
			bac.IncrementProgress();
		}
		( rp.props=&CUnknownDos::Properties )->fnRecognize( rp.image, rp.pOutFormatBoot ); // just a formality to properly fill up the FormatBoot
		return bac.TerminateWithSuccess();
	}

	CDos::PCProperties CDos::CRecognition::Perform(PImage image,PFormat pOutFormatBoot) const{
		// returns Properties of DOS recognized in the specified Image (populates the output Format recognized in the boot Sector); returns UnknownDos if no DOS can be recognized; returns Null if recognition sequence cancelled by the user
		TRecognitionParams rp( *this, image, pOutFormatBoot );
		return	CBackgroundActionCancelable(
					Thread,
					&rp,
					THREAD_PRIORITY_BELOW_NORMAL
				).Perform()==ERROR_SUCCESS
				? rp.props
				: nullptr;
	}

	bool CDos::CRecognition::EditSequence(){
		// True <=> the new order of DOSes that get crack on the disk has been confirmed, otherwise False
		// - defining the Dialog
		class CAutomaticRecognitionOrderDialog sealed:public Utils::CRideDialog{
		public:
			CDos::CRecognition recognition;
		private:
			const Utils::CRideFont &symbolFont;
			CDos::PCProperties newlyDetectedDoses[256];

			void __repopulateListBoxesAndUpdateInteractivity__() const{
				// reads the Recognition Order and (re)populates with it the ListBoxes
				BYTE i=0;
				CDos::PCProperties props;
				CListBox lb;
				// . populating the ListBox with DOSes that take part in the recognition process
				lb.Attach( GetDlgItemHwnd(ID_DOS) );
					// : populating
					int scrollY=lb.GetTopIndex(), iSelected=lb.GetCurSel();
						lb.ResetContent();
						while (( props=recognition.order[++i] )->IsKnown()) // indexing starts from 1
							lb.SetItemDataPtr( lb.AddString(props->name), (PVOID)props );
					lb.SetTopIndex(scrollY), lb.SetCurSel(iSelected);
					// : updating interaction possibilities
					EnableDlgItem( ID_UP, iSelected>0 );
					EnableDlgItem( ID_DOWN, iSelected>=0 && iSelected<lb.GetCount()-1 );
					EnableDlgItem( ID_REMOVE, iSelected>=0 );
					EnableDlgItem( ID_ORDER, lb.GetCount()>0 );
				lb.Detach();
				// . populating the ListBox with DOSes that don't take part in the recognition process
				lb.Attach( GetDlgItemHwnd(ID_HIDDEN) );
					// : populating
					scrollY=lb.GetTopIndex(), iSelected=lb.GetCurSel();
						lb.ResetContent();
						while (++i<=recognition.nDoses){
							props=recognition.order[i];
							lb.SetItemDataPtr( lb.AddString(props->name), (PVOID)props );
						}
					lb.SetTopIndex(scrollY), lb.SetCurSel(iSelected);
					// : updating interaction possibilities
					EnableDlgItem( ID_ADD, iSelected>=0 );
				lb.Detach();
			}

			void PreInitDialog() override{
				// dialog initialization
				// - base
				__super::PreInitDialog();
				// - searching for newly detected DOSes
				CDos::PCProperties *pNewlyDetectedDos=newlyDetectedDoses;
				for( POSITION pos=CDos::Known.GetHeadPosition(); pos; ){
					const CDos::PCProperties props=CDos::Known.GetNext(pos);
					if (!recognition.GetOrderIndex(props))
						recognition.AddDosByPriorityDescending( *pNewlyDetectedDos++=props );
				}
				*pNewlyDetectedDos=nullptr; // terminating the array
				//recognition.SaveToProfile();
				if (pNewlyDetectedDos>newlyDetectedDoses)
					Utils::Information( _T("Some new DOSes have been detected!\nPlease confirm the auto-recognition sequence where they've all been added to (and marked).") );
				// - populating the ListBoxes with current Recognition Order
				__repopulateListBoxesAndUpdateInteractivity__();
				// - setting graphical symbols to buttons that organize the Recognition Order
				SetDlgItemSingleCharUsingFont( ID_UP, 0xf0e1, symbolFont );
				SetDlgItemSingleCharUsingFont( ID_DOWN, 0xf0e2, symbolFont );
				SetDlgItemSingleCharUsingFont( ID_ADD, 0xf0e7, symbolFont );
				SetDlgItemSingleCharUsingFont( ID_REMOVE, 0xf0e8, symbolFont );
				// - (dis)allowing the Cancel button
				#ifndef _DEBUG
					EnableDlgItem( IDCANCEL, *newlyDetectedDoses==nullptr ); // if NewDoses were detected, the Dialog cannot be cancelled - Recognition Order with NewDetectedDoses must be confirmed
				#endif
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_MEASUREITEM:
						// determining the height of a ListBox item (presumed that there are no other owner-drawn controls!)
						((PMEASUREITEMSTRUCT)lParam)->itemHeight=Utils::LogicalUnitScaleFactor*38;
						return 0;
					case WM_DRAWITEM:{
						// drawing the ListBox item (presumed that there are no other owner-drawn controls!)
						const PDRAWITEMSTRUCT pdis=(PDRAWITEMSTRUCT)lParam;
						if (pdis->itemID<0) return 0; // if the ListBox is empty, we are done
						if (!(pdis->itemAction&(ODA_SELECT|ODA_DRAWENTIRE))) return 0; // if no need to draw anything, we are done
						const CDos::PCProperties props=(CDos::PCProperties)pdis->itemData;
						const HDC dc=pdis->hDC;
						::SetBkMode(dc,TRANSPARENT);
						Utils::ScaleLogicalUnit(dc);
						RECT r=pdis->rcItem;
						Utils::UnscaleLogicalUnit((PINT)&r,4), Utils::UnscaleLogicalUnit((PINT)&pdis->rcItem,4);
						const Utils::CRideFont newDosSymbolFont(FONT_WINGDINGS,180,false,false);
						const HGDIOBJ hFont0=::SelectObject( dc, newDosSymbolFont );
							if (pdis->itemState&ODS_SELECTED){
								::FillRect( dc, &r, Utils::CRideBrush::Selection );
								::SetTextColor( dc, COLOR_WHITE );
							}else{
								::FillRect( dc, &r, Utils::CRideBrush::White );
								::SetTextColor( dc, COLOR_BLACK );
							}
							r.left+=4, r.bottom-=15;
							for( CDos::PCProperties *p=newlyDetectedDoses; *p; )
								if (*p++==props){
									static constexpr WCHAR NewDosSymbol=0xf025;
									::DrawTextW( dc, &NewDosSymbol,1, &r, DT_SINGLELINE|DT_VCENTER );
									r.left+=25;
									break;
								}
						::SelectObject( dc, Utils::CRideFont::StdBold );
							::DrawText( dc, props->name,-1, &r, DT_SINGLELINE|DT_VCENTER );
						::SelectObject( dc, Utils::CRideFont::Small );
							r.top=r.bottom-8, r.bottom=pdis->rcItem.bottom;
							TCHAR desc[80];
							::wsprintf( desc, _T("Recognition priority: %d"), props->recognitionPriority );
							::DrawText( dc, desc,-1, &r, DT_SINGLELINE|DT_VCENTER );
						::SelectObject(dc,hFont0);
						return 0;
					}
					case WM_COMMAND:{
						// processing a command
						CListBox lb;
						lb.Attach( GetDlgItemHwnd(ID_DOS) ); // attaching the ListBox of recognized DOSes right away as most commands work with it
							CDos::PCProperties props=(CDos::PCProperties)lb.GetItemDataPtr(lb.GetCurSel());
							BYTE i=recognition.GetOrderIndex(props); // indexing starts from 1
							switch (wParam){
								case ID_UP:
									// moving selected recognized DOS to an earlier position in the Recognition Order
									recognition.order[i]=recognition.order[i-1];
									recognition.order[--i]=props;
									lb.SetCurSel(--i);
									break;
								case ID_DOWN:
									// moving selected recognized DOS to a later position in the Recognition Order
									recognition.order[i]=recognition.order[i+1];
									lb.SetCurSel(i);
									recognition.order[++i]=props;
									break;
								case ID_ORDER:{
									// ordering automatically recognized DOSes by their RecognitionPriority descending
									const CDos::CRecognition original(recognition);
									const BYTE iUnknownDos=original.GetOrderIndex(&CUnknownDos::Properties);
									::memmove(	&recognition.order[1], // indexing starts from 1
												&recognition.order[iUnknownDos],
												sizeof(CDos::PCProperties)*(( recognition.nDoses-=iUnknownDos-1 ))
											);
									for( BYTE j=1; j<iUnknownDos; recognition.AddDosByPriorityDescending(original.order[j++]) );
									if (i){ // if any of recognized DOSes selected before clicking on the button ...
										for( i=1; recognition.order[i]!=props; i++ );
										lb.SetCurSel(i-1); // ... reselecting the DOS after they've been ordered by priority
									}
									break;
								}
								case ID_ADD:{
									// adding selected ignored DOS to the Recognition Order
									// . removing the DOS from "ignored portion" of the Recognition Order
									CListBox lbIgnored;
									lbIgnored.Attach( GetDlgItemHwnd(ID_HIDDEN) );
										props=(CDos::PCProperties)lbIgnored.GetItemDataPtr(lbIgnored.GetCurSel());
										i=recognition.GetOrderIndex(props); // indexing starts from 1
										::memmove(	&recognition.order[i],
													&recognition.order[i+1],
													sizeof(CDos::PCProperties)*((recognition.nDoses--)-i)
												);
										lbIgnored.SetCurSel( std::min(lbIgnored.GetCurSel(),lbIgnored.GetCount()-2) );
									lbIgnored.Detach();
									// . adding (and selecting) the DOS to the "recognized portion" of the Recognition Order
									lb.SetItemDataPtr( lb.AddString(_T("")), (PVOID)&CUnknownDos::Properties ); // a dummy item for the below selection index to be valid
									lb.SetCurSel( recognition.AddDosByPriorityDescending(props)-1 );
									break;
								}
								case ID_REMOVE:{
									// removing selected recognized DOS from the Recognition Order
									const BYTE iUnknownDos=recognition.GetOrderIndex(&CUnknownDos::Properties);
									::memmove(	&recognition.order[i],
												&recognition.order[i+1],
												sizeof(CDos::PCProperties)*(iUnknownDos-i)
											);
									recognition.order[iUnknownDos]=props;
									lb.SetCurSel( std::min(i-1,lb.GetCount()-2) );
									break;
								}
							}
						lb.Detach(); // detaching the above attached ListBox
						__repopulateListBoxesAndUpdateInteractivity__(); // "redrawing" the dialog
						break;
					}
					case WM_NOTIFY:
						// processing a notification
						switch (GetClickedHyperlinkId(lParam)){
							case ID_HELP_INDEX:{
								// . defining the Dialog
								class CHelpDialog sealed:public Utils::CCommandDialog{
									BOOL OnInitDialog() override{
										// dialog initialization
										// : base
										const BOOL result=__super::OnInitDialog();
										// : supplying available actions
										AddHelpButton( ID_DRIVE, _T("What is a recognition sequence good for?") );
										AddCancelButton( MSG_HELP_CANCEL );
										return result;
									}
								public:
									CHelpDialog()
										// ctor
										: Utils::CCommandDialog(_T("This might interest you:")) {
									}
								} d;
								// . showing the Dialog and processing its result
								switch (d.DoModal()){
									case ID_DRIVE:
										Utils::NavigateToFaqInDefaultBrowser( _T("recognition") );
										break;
								}
								return TRUE;
							}
						}
						break;
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			CAutomaticRecognitionOrderDialog()
				// ctor
				: Utils::CRideDialog(IDR_DOS_RECOGNITION)
				, symbolFont(Utils::CRideFont::Wingdings105) {
			}
		} d;
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			d.recognition.SaveToProfile();
			return true;
		}else
			return false;
	}
