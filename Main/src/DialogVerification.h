#ifndef VERIFICATIONDIALOG_H
#define VERIFICATIONDIALOG_H

	#define FAT_VERIFICATION_READABILITY _T("FAT readability")
	#define FAT_VERIFICATION_CROSSLINKED _T("Cross-linked files")
	#define FAT_VERIFICATION_LOSTSECTORS _T("Lost sectors")
	#define FILESYSTEM_VERIFICATION		_T("Filesystem")
	#define SURFACE_VERIFICATION		_T("Surface verification")
	#define VERIFICATION_WARNING_SIGNIFICANT_PROBLEM	_T("There seems to be a significant problem with the disk - this step may take unpredictably longer to finish.\n\nTerminate it and proceed with the next step?")

	#define VERIF_CYLINDER_COUNT		_T("Number of cylinders")
	#define VERIF_HEAD_COUNT			_T("Number of heads")
	#define VERIF_SECTOR_COUNT			_T("Number of sectors")
	#define VERIF_SECTOR_FREE_COUNT		_T("Free sectors")
	#define VERIF_CLUSTER_SIZE			_T("Cluster size")
	#define VERIF_FILE_COUNT			_T("Count of files")

	#define VERIF_VOLUME_NAME			_T("Volume name")
	#define VERIF_DIRECTORY_NAME		_T("Directory name")
	#define VERIF_FILE_NAME				_T("File name")

	#define VERIF_MSG_FAT_SECTOR_BAD	_T("FAT sector with %s is bad")
	#define VERIF_MSG_DIR_SECTOR_BAD	_T("Directory sector with %s is bad")
	#define VERIF_MSG_FILE_NONSTANDARD	_T("%s: Non-standard file type")
	#define VERIF_MSG_ITEM_INTEGRITY_ERR _T("%s: Integrity error")
	#define VERIF_MSG_ITEM_BAD_SECTORS	_T("%s: Not all sectors are readable")
	#define VERIF_MSG_ITEM_BAD_LENGTH	_T("%s: Length incorrect")
	#define VERIF_MSG_ITEM_FAT_ERROR	_T("%s: FAT error (%s)")
	#define VERIF_MSG_ITEM_NO_SECTORS	_T("%s: No sectors affiliated")
	#define VERIF_MSG_DRIVE_ERROR		_T("An error is reported to have occured for current drive")

	#define VERIF_MSG_BAD_SECTOR_EXCLUDE	_T("Bad sectors should be excluded.")
	#define VERIF_MSG_CHECKSUM_RECALC		_T("Checksum should be recomputed.")
	#define VERIF_MSG_FILE_UNCROSS			_T("Kept cross-linked, changes in one will affect the other")
	#define VERIF_MSG_FILE_LENGTH_FROM_FAT	_T("Length should be adopted from FAT.")
	#define VERIF_MSG_FILE_DELETE			_T("File should be deleted")


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




	class CVerifyVolumeDialog sealed:public Utils::CRideDialog{
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
				TFdcStatus sr;
				if (const PCSectorData data=dos->image->GetSectorData(chs,0,Revolution::CURRENT,nullptr,&sr))
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
				TFdcStatus sr;
				if (const PCSectorData data=dos->image->GetSectorData(chs,0,Revolution::CURRENT,nullptr,&sr))
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
