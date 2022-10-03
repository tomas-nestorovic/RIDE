#ifndef SCP_H
#define SCP_H

	class CSCP sealed:public CSuperCardProBase{
		mutable CFile f;

		DWORD tdhOffsets[84][2]; // Track Data Header offsets

		PInternalTrack GetInternalTrackSafe(TCylinder cyl,THead head) const;
		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap) override;
	public:
		static const TProperties Properties;

		CSCP();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
		TStdWinError Reset() override;
	};

#endif // SCP_H
