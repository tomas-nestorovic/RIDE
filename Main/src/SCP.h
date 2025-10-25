#ifndef SCP_H
#define SCP_H

	class CSCP sealed:public CSuperCardProBase{
		mutable CFile f;

		DWORD tdhOffsets[84][2]; // Track Data Header offsets

		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap) override;
	public:
		static const TProperties Properties;

		CSCP();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const;
		const CTrackReader &ReadTrack(TCylinder cyl,THead head) const override;
		TStdWinError Reset() override;
	};

#endif // SCP_H
