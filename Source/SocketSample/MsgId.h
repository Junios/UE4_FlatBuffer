enum class MsgId : unsigned short {
	C2S_Login = 1,
	S2C_Login,
	S2C_SpawnActors,
	S2C_DestroyActor,
	C2S_SyncLocation,
	S2C_SyncLocation,
};
