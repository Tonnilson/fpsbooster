class UIElement
{
	public:
	#ifdef _M_X64
		char pad_0x0000[0x34];
		__int32 ID;
		char pad_0x0038[0x28];
		__int32 X;
		__int32 Y;
		char pad_0x0068[0x1C];
	#else
		char pad_0x0000[0x20];
		__int32 ID;
		char pad_0x0024[0x3C];
	#endif
		__int8 Visibility;
		char pad_0x0085[0x3A];
};