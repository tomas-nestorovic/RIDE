#ifndef VERIFICATIONDIALOG_H
#define VERIFICATIONDIALOG_H

	#define FAT_VERIFICATION_READABILITY _T("FAT readability")
	#define FILESYSTEM_VERIFICATION		_T("Filesystem")
	#define VERIFICATION_WARNING_SIGNIFICANT_PROBLEM	_T("There seems to be a significant problem with the disk - this step may take unpredictably longer to finish.\n\nTerminate it and proceed with the next step?")

	struct TVerificationFunctions sealed{
		static UINT AFX_CDECL ReportOnFilesWithBadFatPath_thread(PVOID pCancelableAction);
		static UINT AFX_CDECL FloppyCrossLinkedFilesVerification_thread(PVOID pCancelableAction);
		static UINT AFX_CDECL FloppyLostSectorsVerification_thread(PVOID pCancelableAction);

		static UINT AFX_CDECL WholeDiskSurfaceVerification_thread(PVOID pCancelableAction);

		AFX_THREADPROC fnBootSector;
		AFX_THREADPROC fnFatFullyReadable;
		AFX_THREADPROC fnFatFilePathsOk;
		AFX_THREADPROC fnFatCrossedFiles;
		AFX_THREADPROC fnFatLostAllocUnits;
		AFX_THREADPROC fnFilesystem;
		AFX_THREADPROC fnVolumeSurface;
	};




	class CVerifyVolumeDialog sealed:public CDialog{
		BYTE nOptionsChecked;

		void DoDataExchange(CDataExchange *pDX) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		struct TParams{
			const PDos dos;
			mutable CBackgroundMultiActionCancelable action;
			int verifyBootSector, verifyFat, verifyFilesystem;
			int verifyVolumeSurface;
			int repairStyle;
			const TVerificationFunctions verificationFunctions;
			mutable class CReportFile sealed:public CFile{
				bool itemListBegun, problemOpen, inProblemSolvingSection;
			public:
				CReportFile();

				void OpenSection(LPCTSTR name,bool problemSolving=true);
				void CloseSection(LPCTSTR errMsg=nullptr);
				void AFX_CDECL LogWarning(LPCTSTR format,...);
				//void LogError(
				void OpenProblem(LPCTSTR problemDesc);
				void CloseProblem(bool solved);
				void Close() override;
			} fReport;

			TParams(CDos *dos,const TVerificationFunctions &rvf);
			TParams(const TParams &);

			template<typename T>
			TStdWinError TerminateAndGoToNextAction(T error) const;
			TStdWinError TerminateAll(TStdWinError error) const;
			TStdWinError CancelAll() const;
			BYTE ConfirmFix(LPCTSTR problemDesc,LPCTSTR problemSolvingSuggestion) const;
			BYTE ConfirmUnsignedValueFix(LPCTSTR locationName,LPCTSTR valueName,WORD valueOffset,DWORD value,DWORD rangeA,DWORD rangeZ) const;
			bool WarnIfUnsignedValueOutOfRange(LPCTSTR locationName,LPCTSTR valueName,WORD valueOffset,DWORD value,DWORD rangeA,DWORD rangeZ) const;

			template<typename T>
			BYTE ConfirmUnsignedValueFix(LPCTSTR locationName,LPCTSTR valueName,LPCVOID locationBegin,const T &rValue,T rangeA,T rangeZ) const{
				// confirms an unsigned numeric value at specified Offset has a Value from Range={A,...,Z}; if not, presents the problem using standard formulation and returns user's reaction
				return	ConfirmUnsignedValueFix(
							locationName, valueName,
							(PCBYTE)&rValue-(PCBYTE)locationBegin,
							rValue,
							rangeA, rangeZ
						);
			}

			template<typename T>
			TStdWinError VerifyUnsignedValue(RCPhysicalAddress chs,LPCTSTR chsName,LPCTSTR valueName,T &rValue,T rangeA,T rangeZ) const{
				// confirms an unsigned numeric value at specified Offset has a Value from Range={A,...,Z}; if not, presents the problem using standard formulation and returns user's reaction
				WORD w; TFdcStatus sr;
				if (const PCSectorData data=dos->image->GetSectorData(chs,0,false,&w,&sr))
					if (sr.IsWithoutError())
						switch (ConfirmUnsignedValueFix(
									chsName, valueName,
									(PCBYTE)&rValue-data,
									rValue,
									rangeA, rangeZ
								)
						){
							case IDYES:
								if (rValue<rangeA)
									rValue=rangeA;
								else if (rValue>rangeZ)
									rValue=rangeZ;
								dos->image->MarkSectorAsDirty(chs);
								fReport.CloseProblem(true);
								//fallthrough
							case IDNO:
								return ERROR_SUCCESS; // even if fix rejected, this value has been verified
							default:
								return ERROR_CANCELLED;
						}
				return ::GetLastError();
			}

			template<typename T>
			TStdWinError VerifyUnsignedValue(RCPhysicalAddress chs,LPCTSTR chsName,LPCTSTR valueName,T &rValue,T onlyValidValue) const{
				// confirms an unsigned numeric value at specified Offset has a Value from Range={A,...,Z}; if not, presents the problem using standard formulation and returns user's reaction
				return VerifyUnsignedValue( chs, chsName, valueName, rValue, onlyValidValue, onlyValidValue );
			}

			template<typename T>
			TStdWinError WarnIfUnsignedValueOutOfRange(RCPhysicalAddress chs,LPCTSTR chsName,LPCTSTR valueName,const T &rValue,T rangeA,T rangeZ) const{
				// issues a warning if unsigned numeric value at specified Offset has a Value out of Range={A,...,Z}; returns Windows standard i/o error
				WORD w; TFdcStatus sr;
				if (const PCSectorData data=dos->image->GetSectorData(chs,0,false,&w,&sr))
					if (sr.IsWithoutError())
						return	WarnIfUnsignedValueOutOfRange(
									chsName, valueName,
									(PCBYTE)&rValue-data,
									rValue,
									rangeA, rangeZ
								)
								? ERROR_INVALID_PARAMETER
								: ERROR_SUCCESS;
				return ::GetLastError();
			}
		} &params;

		CVerifyVolumeDialog(TParams &rvp);
	};

#endif // VERIFICATIONDIALOG_H
