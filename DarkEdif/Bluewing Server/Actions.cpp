
#include "Common.h"
#ifdef _DEBUG
std::stringstream CriticalSection;
#endif

#define Remake(name) MessageBoxA(NULL, "Your "#name" actions need to be recreated.\n" \
										"This is probably due to parameter changes.", "Lacewing Blue Server", MB_OK)
#define EventsToRun globals->_eventsToRun

static char errtext[1024];
void ErrNoToErrText()
{
	int error = errno; // strerror_s may change errno
	if (strerror_s(errtext, error))
	{
		strcpy_s(errtext, "errno failed to convert");
		_set_errno(error);
	}
}


void Extension::RemovedActionNoParams()
{
	CreateError("Action needs removing.");
}
void Extension::RelayServer_Host(int port)
{
	if (Srv.hosting())
		return CreateError("Cannot start hosting: already hosting a server.");

	Srv.host(port);
}
void Extension::RelayServer_StopHosting()
{
	Srv.unhost();
}
void Extension::FlashServer_Host(const TCHAR * path)
{
	if (FlashSrv->hosting())
		return CreateError("Cannot start hosting flash policy: already hosting a flash policy.");

	FlashSrv->host(TStringToUTF8(path).c_str());
}
void Extension::FlashServer_StopHosting()
{
	FlashSrv->unhost();
}
void Extension::HTML5Server_EnableHosting()
{

}
void Extension::HTML5Server_DisableHosting()
{

}
void Extension::ChannelListing_Enable()
{
	Srv.setchannellisting(true);
}
void Extension::ChannelListing_Disable()
{
	Srv.setchannellisting(false);
}
void Extension::SetWelcomeMessage(const TCHAR * message)
{
	Srv.setwelcomemessage(TStringToUTF8(message));
}
void Extension::SetUnicodeAllowList(const TCHAR * listToSet, const TCHAR * allowListContents)
{
	static const std::tstring_view listNames[]{
		_T("clientnames"sv),
		_T("channelnames"sv),
		_T("receivedbyclient"sv),
		_T("receivedbyserver"sv),
		// Portuguese
		_T("nomedocliente"sv),
		_T("nomedocanal"sv),
		_T("recebidopelocliente"sv),
		_T("recebidopeloservidor"sv),
		// French
		_T("nomduclient"sv),
		_T("nomducanal"sv)
		_T("reçuparleclient"sv),
		_T("reçuparleserveur"sv)
	};
	
	std::tstring listToSetStr(listToSet);

	std::transform(listToSetStr.begin(), listToSetStr.end(), listToSetStr.begin(),
		[](const TCHAR c) { return ::_totlower(c); });

	for (size_t i = 0; i < std::size(listNames); i++)
	{
		if (listNames[i] == listToSetStr)
		{
			std::string err = Srv.setcodepointsallowedlist((lacewing::relayserver::codepointsallowlistindex)(i % 4), TStringToANSI(allowListContents));
			if (!err.empty())
				CreateError("Couldn't set Unicode %s allow list, %hs.", TStringToUTF8(listToSet).c_str(), err.c_str());
			return;
		}
	}

	CreateError(R"(Unicode allow list %s does not exist, should be "client names", "channel names", "received by client" or "received by server".)", TStringToUTF8(listToSet).c_str());
}


static AutoResponse ConvToAutoResponse(int informFusion, int immediateRespondWith,
	const char * & denyReason, GlobalInfo * globals, const char * const funcName)
{
	static char err[256];

	// Settings:
	// Auto approve, later inform Fusion [1, 0]
	// Auto deny, later inform Fusion [1, 1]
	// Wait for Fusion to say yay or nay [1, 2]
	// Auto approve, say nothing to Fusion [0, 0]
	// Auto deny, say nothing to Fusion [0, 1]
	// Do nothing, say nothing to Fusion [0, 2] -> not usable!

	if (informFusion < 0 || informFusion > 1)
		sprintf_s(err, sizeof(err), "Invalid \"Inform Fusion\" parameter passed to \"enable/disable condition: %hs\".", funcName);
	else if (immediateRespondWith < 0 || immediateRespondWith > 2)
		sprintf_s(err, sizeof(err), "Invalid \"Immediate Respond With\" parameter passed to \"enable/disable condition: %hs\".", funcName);
	else if (informFusion == 0 && immediateRespondWith == 2)
		sprintf_s(err, sizeof(err), "Invalid parameters passed to \"enable/disable condition: %hs\"; with no immediate response"
			" and Fusion condition triggering off, the server wouldn't know what to do.", funcName);
	else
	{
		// If we're not denying, replace deny parameter with null
		if (immediateRespondWith != 1)
			denyReason = "";

		AutoResponse autoResponse = AutoResponse::Invalid;
		if (informFusion == 1)
		{
			if (immediateRespondWith == 0)
				autoResponse = AutoResponse::Approve_TellFusion;
			else if (immediateRespondWith == 1)
				autoResponse = AutoResponse::Deny_TellFusion;
			else /* immediateRespondWith == 2 */
				autoResponse = AutoResponse::WaitForFusion;
		}
		else /* informFusion == 0 */
		{
			if (immediateRespondWith == 0)
				autoResponse = AutoResponse::Approve_Quiet;
			else /* if (immediateRespondWith == 1) */
				autoResponse = AutoResponse::Deny_Quiet;
			/* immediateRespondWith == 2 is invalid with informFusion = 0 */;
		}

		return autoResponse;
	}

	globals->CreateError(err);
	return AutoResponse::Invalid;
}


void Extension::EnableCondition_OnConnectRequest(int informFusion, int immediateRespondWith, const TCHAR * autoDenyReason)
{
	const std::string autoDenyReasonU8 = TStringToUTF8(autoDenyReason);
	const char * autoDenyReasonU8CStr = autoDenyReasonU8.c_str();
	AutoResponse resp = ConvToAutoResponse(informFusion, immediateRespondWith,
		autoDenyReasonU8CStr, globals, "on connect request");
	if (resp == AutoResponse::Invalid)
		return;
	globals->autoResponse_Connect = resp;
	globals->autoResponse_Connect_DenyReason = TStringToUTF8(autoDenyReason);
	Srv.onconnect(::OnClientConnectRequest);
}
void Extension::EnableCondition_OnNameSetRequest(int informFusion, int immediateRespondWith, const TCHAR * autoDenyReason)
{
	const std::string autoDenyReasonU8 = TStringToUTF8(autoDenyReason);
	const char * autoDenyReasonU8CStr = autoDenyReasonU8.c_str();
	AutoResponse resp = ConvToAutoResponse(informFusion, immediateRespondWith,
		autoDenyReasonU8CStr, globals, "on name set request");
	if (resp == AutoResponse::Invalid)
		return;
	globals->autoResponse_NameSet = resp;
	globals->autoResponse_NameSet_DenyReason = TStringToUTF8(autoDenyReason);
	Srv.onnameset(resp != AutoResponse::Approve_Quiet ? ::OnNameSetRequest : nullptr);
}
void Extension::EnableCondition_OnJoinChannelRequest(int informFusion, int immediateRespondWith, const TCHAR * autoDenyReason)
{
	const std::string autoDenyReasonU8 = TStringToUTF8(autoDenyReason);
	const char * autoDenyReasonU8CStr = autoDenyReasonU8.c_str();
	AutoResponse resp = ConvToAutoResponse(informFusion, immediateRespondWith,
		autoDenyReasonU8CStr, globals, "on join channel request");
	if (resp == AutoResponse::Invalid)
		return;
	globals->autoResponse_ChannelJoin = resp;
	globals->autoResponse_ChannelJoin_DenyReason = TStringToUTF8(autoDenyReason);
	Srv.onchannel_join(resp != AutoResponse::Approve_Quiet ? ::OnJoinChannelRequest : nullptr);
}
void Extension::EnableCondition_OnLeaveChannelRequest(int informFusion, int immediateRespondWith, const TCHAR * autoDenyReason)
{
	const std::string autoDenyReasonU8 = TStringToUTF8(autoDenyReason);
	const char * autoDenyReasonU8CStr = autoDenyReasonU8.c_str();
	AutoResponse resp = ConvToAutoResponse(informFusion, immediateRespondWith,
		autoDenyReasonU8CStr, globals, "on join channel request");
	if (resp == AutoResponse::Invalid)
		return;
	globals->autoResponse_ChannelLeave = resp;
	globals->autoResponse_ChannelLeave_DenyReason = TStringToUTF8(autoDenyReason);
	// If local data for channel is used at all, we don't want it dangling, so make sure OnLeave is always ran.
	Srv.onchannel_leave(::OnLeaveChannelRequest);
}
void Extension::EnableCondition_OnMessageToPeer(int informFusion, int immediateRespondWith)
{
	const char * dummyDenyReason = "X";
	AutoResponse resp = ConvToAutoResponse(informFusion, immediateRespondWith,
		dummyDenyReason, globals, "on join channel request");
	if (resp == AutoResponse::Invalid)
		return;
	globals->autoResponse_MessageClient = resp;
	Srv.onmessage_peer(resp != AutoResponse::Approve_Quiet ? ::OnPeerMessage : nullptr);
}
void Extension::EnableCondition_OnMessageToChannel(int informFusion, int immediateRespondWith)
{
	const char * dummyDenyReason = "X";
	AutoResponse resp = ConvToAutoResponse(informFusion, immediateRespondWith,
		dummyDenyReason, globals, "on join channel request");
	if (resp == AutoResponse::Invalid)
		return;
	globals->autoResponse_MessageChannel = resp;
	Srv.onmessage_channel(resp != AutoResponse::Approve_Quiet ? ::OnChannelMessage : nullptr);
}
void Extension::EnableCondition_OnMessageToServer(int informFusion)
{
	if (informFusion < 0 || informFusion > 1)
		return globals->CreateError("Invalid \"Inform Fusion\" parameter passed to \"enable/disable condition: on message to server\".");

	// This one's handled a bit differently; there is no auto approve/deny.
	// The message is either read by Fusion or discarded immediately.
	globals->autoResponse_MessageServer = informFusion == 1 ? AutoResponse::WaitForFusion : AutoResponse::Deny_Quiet;
	Srv.onmessage_server(informFusion == 1 ? ::OnServerMessage : nullptr);
}
void Extension::OnInteractive_Deny(const TCHAR * reason)
{
	if (InteractivePending == InteractiveType::None)
		return CreateError("Cannot deny client's action: No interactive action is pending.");
	// All of the interactive events currently allow denying
	//else if ((InteractivePending & InteractiveType::DenyPermitted) != InteractiveType::DenyPermitted)
	//	return CreateError("Cannot deny client's action: Interactive event is not compatible with this action.");
	if (!DenyReason.empty())
		return CreateError("Can't deny client's action: Set to auto-deny, or Deny was called more than once. Ignoring additional denies.");

	DenyReason = reason[0] ? TStringToUTF8(reason) : "No reason specified."s;
}
void Extension::OnInteractive_ChangeClientName(const TCHAR * newName)
{
	if (newName[0] == _T('\0'))
		return CreateError("Cannot change new client name: Cannot use a blank name.");

	std::string newNameU8 = TStringToUTF8(newName);
	if (newNameU8.empty() || !lw_u8str_validate(newNameU8) || !lw_u8str_normalise(newNameU8) ||
		!Srv.checkcodepointsallowed(lacewing::relayserver::codepointsallowlistindex::ClientNames, newNameU8))
	{
		return CreateError("Cannot change new client name: Invalid characters in new name.");
	}
	if (newNameU8.size() > 255)
		return CreateError("Cannot change new client name: Cannot use a name longer than 255 characters (after UTF-8 conversion).");

	if (InteractivePending == InteractiveType::None)
		return CreateError("Cannot change new client name: No interactive action is pending.");
	if (InteractivePending != InteractiveType::ClientNameSet)
		return CreateError("Cannot change new client name: Interactive event is not compatible with this action.");
	if (!DenyReason.empty())
		return CreateError("Cannot change new client name: Name change has already been denied by the Deny Request action.");
	if (lw_sv_cmp(NewClientName, newNameU8))
		return CreateError("Cannot change new channel name: New name is same as original name.");

	NewClientName = newNameU8;
}
void Extension::OnInteractive_ChangeChannelName(const TCHAR * newName)
{
	if (newName[0] == _T('\0'))
		return CreateError("Cannot change joining channel name: Cannot use a blank name.");

	std::string newNameU8 = TStringToUTF8(newName);
	if (newNameU8.empty() || !lw_u8str_validate(newNameU8) || !lw_u8str_normalise(newNameU8) ||
		!Srv.checkcodepointsallowed(lacewing::relayserver::codepointsallowlistindex::ChannelNames, newNameU8))
		return CreateError("Cannot change joining channel name: Invalid characters in new name.");
	if (newNameU8.size() > 255)
		return CreateError("Cannot change joining channel name: Cannot use a name longer than 255 characters (after UTF-8 conversion).");

	if (InteractivePending == InteractiveType::None)
		return CreateError("Cannot change joining channel name: No interactive action is pending.");
	if ((InteractivePending & InteractiveType::ChannelJoin) != InteractiveType::ChannelJoin)
		return CreateError("Cannot change joining channel name: Interactive event is not compatible with this action.");
	if (!DenyReason.empty())
		return CreateError("Cannot change joining channel name: Channel name join has already been denied by the Deny Request action.");
	if (lw_sv_cmp(NewChannelName, newNameU8))
		return CreateError("Cannot change new channel name: New name is same as original name.");

	NewChannelName = newNameU8;
}
void Extension::OnInteractive_DropMessage()
{
	if (InteractivePending == InteractiveType::None)
		return CreateError("Cannot deny the action: No interactive action is pending.");
	if (InteractivePending != InteractiveType::ChannelMessageIntercept &&
		InteractivePending != InteractiveType::ClientMessageIntercept)
	{
		return CreateError("Cannot drop message: Interactive event is not compatible with this action.");
	}
	if (DropMessage)
		return CreateError("Error dropping message: Message already being dropped.");
	DropMessage = true;
}
void Extension::OnInteractive_ReplaceMessageWithText(const TCHAR * NewText)
{
	if (InteractivePending == InteractiveType::None)
		return CreateError("Cannot deny the action: No interactive action is pending.");
	if (InteractivePending != InteractiveType::ChannelMessageIntercept &&
		InteractivePending != InteractiveType::ClientMessageIntercept)
	{
		return CreateError("Cannot replace message: Interactive event is not compatible with this action.");
	}
	if (DropMessage)
		return CreateError("Cannot replace message: Message was dropped.");

	// See the Decompress Received Binary for implementation. Also, see
	// !Srv.checkcodepointsallowed(lacewing::relayserver::codepointsallowlistindex::RecvByClientMessages, newNameU8)
	return CreateError("Cannot replace message: Replacing messages not implemented.");
}
void Extension::OnInteractive_ReplaceMessageWithNumber(int newNumber)
{
	if (InteractivePending == InteractiveType::None)
		return CreateError("Cannot deny the action: No interactive action is pending.");
	if (InteractivePending != InteractiveType::ChannelMessageIntercept &&
		InteractivePending != InteractiveType::ClientMessageIntercept)
	{
		return CreateError("Cannot replace message: Interactive event is not compatible with this action.");
	}
	if (DropMessage)
		return CreateError("Cannot replace message: Message was dropped.");

	// See the Decompress Received Binary for implementation.
	return CreateError("Cannot replace message: Replacing messages not implemented.");
}
void Extension::OnInteractive_ReplaceMessageWithSendBinary()
{
	if (InteractivePending == InteractiveType::None)
		return CreateError("Cannot deny the action: No interactive action is pending.");
	if (InteractivePending != InteractiveType::ChannelMessageIntercept &&
		InteractivePending != InteractiveType::ClientMessageIntercept)
	{
		return CreateError("Cannot replace message: Interactive event is not compatible with this action.");
	}
	if (DropMessage)
		return CreateError("Cannot replace message: Message was dropped.");

	// See the Decompress Received Binary for implementation.
	CreateError("Cannot replace message: Replacing messages not implemented.");
}

void Extension::Channel_SelectByName(const TCHAR * channelNamePtr)
{
	if (channelNamePtr[0] == _T('\0'))
		return CreateError("Channel_SelectByName() was called with a blank name.");

	const std::string channelName(TStringToUTF8(channelNamePtr));
	if (channelName.size() > 255U)
		return CreateError("Channel_SelectByName() was called with a name exceeding the max length of 255 characters.");

	const std::string channelNameSimplified = lw_u8str_simplify(channelName.c_str());
	selChannel = nullptr;
	{
		lacewing::readlock serverReadLock = Srv.lock.createReadLock();
		const auto& channels = Srv.getchannels();
		for (const auto &ch : channels)
		{
			if (lw_sv_cmp(ch->nameSimplified(), channelNameSimplified))
			{
				selChannel = ch;

				if (selClient == nullptr)
					return;

				// If selected client is on new channel, keep it selected, otherwise deselect client
				serverReadLock.lw_unlock();

				auto channelReadLock = ch->lock.createReadLock();
				const auto &clientsOnChannel = ch->getclients();
				if (std::find(clientsOnChannel.cbegin(), clientsOnChannel.cend(), selClient) == clientsOnChannel.cend())
					selClient = nullptr;
				return;
			}
		}
	}

	CreateError("Selecting channel by name failed: Channel with name %s not found on server.", TStringToUTF8(channelNamePtr).c_str());
}
void Extension::Channel_Close()
{
	if (!selChannel)
		return CreateError("Could not close channel: No channel selected.");
	if (selChannel->readonly())
		return CreateError("Could not close channel: Already closing.");

	selChannel->close();
}
void Extension::Channel_SelectMaster()
{
	if (!selChannel)
		return CreateError("Could not select channel master: No channel selected.");

	selClient = selChannel->channelmaster();
}
void Extension::Channel_LoopClients()
{
	if (!selChannel)
		return CreateError("Loop Clients On Channel was called without a channel being selected.");

	auto origSelChannel = selChannel;
	auto origSelClient = selClient;
	auto origLoopName = loopName;

	std::vector<decltype(origSelClient)> channelClientListDup;
	{
		auto channelReadLock = origSelChannel->lock.createReadLock();
		channelClientListDup = origSelChannel->getclients();
	}

	for (const auto &cli : channelClientListDup)
	{
		selChannel = origSelChannel;
		selClient = cli;
		loopName = std::tstring_view();
		Runtime.GenerateEvent(8);
	}
	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = std::tstring_view();

	Runtime.GenerateEvent(44);
	loopName = origLoopName;
}
void Extension::Channel_LoopClientsWithName(const TCHAR * passedLoopName)
{
	if (!selChannel)
		return CreateError("Loop Clients On Channel With Name was called without a channel being selected.");

	// You can loop a closed channel's clients, but it's read-only.
	auto origSelChannel = selChannel;
	auto origSelClient = selClient;
	auto origLoopName = loopName;

	const std::tstring_view loopNameDup(passedLoopName);
	std::vector<decltype(origSelClient)> channelClientListDup;
	{
		auto channelReadLock = origSelChannel->lock.createReadLock();
		channelClientListDup = origSelChannel->getclients();
	}

	for (const auto &cli : channelClientListDup)
	{
		selChannel = origSelChannel;
		selClient = cli;
		loopName = loopNameDup;
		Runtime.GenerateEvent(39);
	}
	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = loopNameDup;

	Runtime.GenerateEvent(40);
	loopName = origLoopName;
}
void Extension::Channel_SetLocalData(const TCHAR * key, const TCHAR * value)
{
	if (!selChannel)
		return CreateError("Could not set channel local data: No channel selected.");
	// if (selChannel->readonly())
	//	return CreateError("Could not set channel local data: Channel is read-only.");

	globals->SetLocalData(selChannel, key, value);
}
void Extension::Channel_CreateChannelWithMasterByName(const TCHAR * channelNamePtr, int hiddenInt, int autocloseInt, const TCHAR * masterClientName)
{
	if (channelNamePtr[0] == _T('\0'))
		return CreateError("Cannot create new channel; blank channel name supplied.");
	if (hiddenInt < 0 || hiddenInt > 1)
		return CreateError("Cannot create new channel; hidden channel setting is %i, should be 0 or 1.", hiddenInt);
	if (autocloseInt < 0 || autocloseInt > 1)
		return CreateError("Cannot create new channel; autoclose channel setting is %i, should be 0 or 1.", autocloseInt);

	const std::string channelNameU8 = TStringToUTF8(channelNamePtr);
	if (channelNameU8.size() > 254)
		return CreateError("Cannot create new channel; channel name \"%s\" is too long (after UTF-8 conversion).", TStringToUTF8(channelNamePtr).c_str());
	const std::string channelNameU8Simplified = lw_u8str_simplify(channelNameU8);

	const bool hidden = hiddenInt == 1, autoclose = autocloseInt == 1;

	{
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & channels = Srv.getchannels();
		auto foundChIt = std::find_if(channels.cbegin(), channels.cend(),
			[&](const auto & cli) {
				return lw_sv_cmp(cli->nameSimplified(), channelNameU8); });
		if (foundChIt != channels.cend())
		{
			return CreateError("Error creating channel with name \"%s\"; channel already exists (matching channel ID %hu, name %s).",
				TStringToUTF8(channelNamePtr).c_str(), (**foundChIt).id(), (**foundChIt).name().c_str());
		}
	}

	// Blank master client
	decltype(selClient) masterClientToUse;
	if (masterClientName == _T('\0'))
	{
		// Autoclose means when master leaves, you leave. Since there's no "set master" action yet...
		// the channel will autoclose immediately on creation, which makes no sense.
		if (autoclose)
			return CreateError("Error creating channel; no master specified, and autoclose \"leave when master leaves\" setting cannot be enabled when there is no master to leave.");
	}
	else // Pick master client
	{
		const std::string masterClientNameU8Simplified = TStringToUTF8Simplified(channelNamePtr);
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[&](const auto & cli) { return lw_sv_cmp(cli->nameSimplified(), masterClientNameU8Simplified); });
		if (foundCliIt == clients.cend())
			return CreateError("Error creating channel; specified master client name \"%s\" not found on server.", TStringToUTF8(masterClientName).c_str());
		selClient = *foundCliIt;
	}

	// Will submit joinchannel_response if needed; also adds to server channel list
	Srv.createchannel(channelNameU8, masterClientToUse, hidden, autoclose);
}
void Extension::Channel_CreateChannelWithMasterByID(const TCHAR * channelNamePtr, int hiddenInt, int autocloseInt, int masterClientID)
{
	if (channelNamePtr[0] == _T('\0'))
		return CreateError("Cannot create new channel; blank channel name supplied.");
	if (hiddenInt < 0 || hiddenInt > 1)
		return CreateError("Cannot create new channel; hidden channel setting is %i, should be 0 or 1.", hiddenInt);
	if (autocloseInt < 0 || autocloseInt > 1)
		return CreateError("Cannot create new channel; autoclose channel setting is %i, should be 0 or 1.", autocloseInt);
	if (masterClientID < -1 || masterClientID >= 65535)
		return CreateError("Cannot create new channel; master client ID %i is invalid. Use -1 for no master.", masterClientID);

	const std::string channelNameU8 = TStringToUTF8(channelNamePtr);
	if (channelNameU8.size() > 254)
		return CreateError("Cannot create new channel; channel name \"%s\" is too long (after UTF-8 conversion).", TStringToUTF8(channelNamePtr).c_str());
	const std::string channelNameU8Simplified = lw_u8str_simplify(channelNameU8);

	bool hidden = hiddenInt == 1, autoclose = autocloseInt == 1;

	{
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & channels = Srv.getchannels();
		auto foundChIt =
			std::find_if(channels.cbegin(), channels.cend(),
				[=](const auto & cli) { return lw_sv_cmp(cli->nameSimplified(), channelNameU8Simplified); });
		if (foundChIt != channels.cend())
			return CreateError("Error creating channel with name \"%s\"; channel already exists.", TStringToUTF8(channelNamePtr).c_str());
	}

	// Blank master client
	decltype(selClient) masterClientToUse;
	if (masterClientID == -1)
	{
		// Autoclose means when master leaves, you leave. Since there's no "set master" action yet... this makes no sense.
		if (autoclose)
			return CreateError("Error creating channel; no master specified, and autoclose \"leave when master leaves\" setting cannot be enabled when there is no master to leave.");
	}
	else // Pick master client
	{
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[=](const auto & cli) { return cli->id() == masterClientID; });
		if (foundCliIt == clients.cend())
			return CreateError("Error creating channel; specified master client ID %i not found on server.", masterClientID);
		selClient = *foundCliIt;
	}

	// Will submit joinchannel_response if needed; also adds to server channel list
	Srv.createchannel(channelNameU8, masterClientToUse, hidden, autoclose);
}
void Extension::Channel_JoinClientByID(int clientID)
{
	if (!selChannel)
		return CreateError("Cannot force client to join channel; no channel selected.");
	if (!selChannel->readonly())
		return CreateError("Error forcing client to join channel; channel is read-only.");
	if (clientID < -1 || clientID >= 65535)
		return CreateError("Cannot join client to channel; supplied client ID %i is invalid. Use -1 for currently selected client.", clientID);

	// Note: if user attempts to connect to "abc", and Fusion dev decides, while handling the event,
	// to join the user to "abc", on server end, this will be auto-denied by sanity checks in second run of joinchannel_response.
	// On client end, this will cause a join channel success, followed by a "error joining channel, you're already on the channel"
	// join channel denied message.

	decltype(selClient) clientToUse = nullptr;
	if (clientID == -1)
	{
		if (!selClient)
			return CreateError("Error joining client to selected channel; ID -1 was supplied and no client currently selected.");
		clientToUse = selClient;
	}
	else
	{
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[=](const auto & cli) { return cli->id() == clientID; });
		if (foundCliIt == clients.cend())
			return CreateError("Error joining client with ID %i from channel; client with that ID not found on server.", clientID);
		clientToUse = *foundCliIt;
	}

	if (clientToUse->readonly())
	{
		return CreateError("Error joining client \"%s\" (ID %i) to channel \"%s\"; client is read-only.",
			clientToUse->name().c_str(), clientID, selChannel->name().c_str());
	}

	// Check that channel does not contain client, and client does not contain channel; there may be inconsistency if client is currently leaving.
	{
		auto chCliReadLock = selChannel->lock.createReadLock();
		const auto & clientsOnChannel = selChannel->getclients();
		if (std::find(clientsOnChannel.cbegin(), clientsOnChannel.cend(), selClient) != clientsOnChannel.cend())
		{
			return CreateError("Error joining client \"%s\" (ID %i) to channel \"%s\"; client is already on the channel.",
				clientToUse->name().c_str(), clientID, selChannel->name().c_str());
		}
	}
	{
		auto cliChReadLock = clientToUse->lock.createReadLock();
		const auto & channelsOnClient = clientToUse->getchannels();
		if (std::find(channelsOnClient.cbegin(), channelsOnClient.cend(), selChannel) != channelsOnClient.cend())
		{
			return CreateError("Error joining client \"%s\" (ID %i) to channel \"%s\"; client is already on the channel.",
				clientToUse->name().c_str(), clientID, selChannel->name().c_str());
		}
	}

	// All checks passed; make it happen
	Srv.joinchannel_response(selChannel, clientToUse, std::string_view());
}
void Extension::Channel_JoinClientByName(const TCHAR * clientNamePtr)
{
	if (!selChannel)
		return CreateError("Cannot join client to channel; no channel selected.");
	if (!selChannel->readonly())
		return CreateError("Error joining client to selected channel; channel is read-only.");

	decltype(selClient) clientToUse = nullptr;
	if (clientNamePtr[0] == _T('\0'))
	{
		if (!selClient)
			return CreateError("Error joining client to selected channel; blank client name was supplied and no client currently selected.");
		clientToUse = selClient;
	}
	else
	{
		const std::string clientNameU8Simplified = TStringToUTF8Simplified(clientNamePtr);
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[&](const auto & cli) { return lw_sv_cmp(cli->nameSimplified(), clientNameU8Simplified); });
		if (foundCliIt == clients.cend())
			return CreateError("Error joining client with name \"%s\" to channel; client with that name not found on server.", TStringToUTF8(clientNamePtr).c_str());
		clientToUse = *foundCliIt;
	}

	if (clientToUse->readonly())
	{
		return CreateError("Error joining client \"%s\" (ID %hu) to channel \"%s\"; client is read-only.",
			clientToUse->name().c_str(), clientToUse->id(), selChannel->name().c_str());
	}

	// Check that channel contains client, and client contains channel; there may be inconsistency if client is currently leaving.
	{
		auto chCliReadLock = selChannel->lock.createReadLock();
		const auto & clientsOnChannel = selChannel->getclients();
		if (std::find(clientsOnChannel.cbegin(), clientsOnChannel.cend(), selClient) != clientsOnChannel.cend())
		{
			return CreateError("Error joining client \"%s\" (ID %hu) to channel \"%s\"; client is already on the channel.",
				clientToUse->name().c_str(), clientToUse->id(), selChannel->name().c_str());
		}
	}
	{
		auto cliChReadLock = clientToUse->lock.createReadLock();
		const auto & channelsOnClient = clientToUse->getchannels();
		if (std::find(channelsOnClient.cbegin(), channelsOnClient.cend(), selChannel) != channelsOnClient.cend())
		{
			return CreateError("Error joining client \"%s\" (ID %hu) to channel \"%s\"; client is already on the channel.",
				clientToUse->name().c_str(), clientToUse->id(), selChannel->name().c_str());
		}
	}

	// All checks passed; make it happen
	Srv.joinchannel_response(selChannel, clientToUse, std::string_view());
}
void Extension::Channel_KickClientByID(int clientID)
{
	if (!selChannel)
		return CreateError("Cannot force client to leave channel; no channel selected.");
	if (!selChannel->readonly())
		return CreateError("Error forcing client to leave channel; channel is read-only.");
	if (clientID < -1 || clientID >= 65535)
		return CreateError("Cannot kick client from channel; supplied client ID %i is invalid. Use -1 for currently selected client.", clientID);

	decltype(selClient) clientToUse = nullptr;

	if (clientID == -1)
	{
		if (!selClient)
			return CreateError("Error kicking client from selected channel; ID -1 was supplied but no client currently selected.");
		clientToUse = selClient;
	}
	else
	{
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[=](const auto & cli) { return cli->id() == clientID; });
		if (foundCliIt == clients.cend())
			return CreateError("Error kicking client with ID %i from channel; client with that ID not found on server.", clientID);
		clientToUse = *foundCliIt;
	}

	if (clientToUse->readonly())
	{
		return CreateError("Error kicking client \"%s\" (ID %i) from channel \"%s\"; client is read-only.",
			clientToUse->name().c_str(), clientID, selChannel->name().c_str());
	}

	// Check that channel contains client, and client contains channel; there may be inconsistency if client is currently leaving.
	{
		auto chCliReadLock = selChannel->lock.createReadLock();
		const auto & clientsOnChannel = selChannel->getclients();
		if (std::find(clientsOnChannel.cbegin(), clientsOnChannel.cend(), selClient) == clientsOnChannel.cend())
		{
			return CreateError("Error kicking client \"%s\" (ID %i) from channel \"%s\"; client is not on the channel.",
				clientToUse->name().c_str(), clientID, selChannel->name().c_str());
		}
	}
	{
		auto cliChReadLock = clientToUse->lock.createReadLock();
		const auto & channelsOnClient = clientToUse->getchannels();
		if (std::find(channelsOnClient.cbegin(), channelsOnClient.cend(), selChannel) == channelsOnClient.cend())
		{
			return CreateError("Error kicking client \"%s\" (ID %i) from channel \"%s\"; client is not on the channel.",
				clientToUse->name().c_str(), clientID, selChannel->name().c_str());
		}
	}

	// All checks passed; make it happen
	Srv.leavechannel_response(selChannel, clientToUse, std::string_view());
}
void Extension::Channel_KickClientByName(const TCHAR * clientNamePtr)
{
	if (!selChannel)
		return CreateError("Cannot force client to leave channel; no channel selected.");
	if (!selChannel->readonly())
		return CreateError("Error forcing client to leave channel; channel is read-only.");

	decltype(selClient) clientToUse = nullptr;
	if (clientNamePtr[0] == _T('\0'))
	{
		if (!selClient)
			return CreateError("Error kicking client from selected channel; blank name was supplied but no client currently selected.");
		clientToUse = selClient;
	}
	else
	{
		const std::string clientNameU8Simplified = TStringToUTF8Simplified(clientNamePtr);
		auto serverReadLock = Srv.lock.createReadLock();
		const auto & clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[&](const auto & cli) { return lw_sv_cmp(cli->nameSimplified(), clientNameU8Simplified); });
		if (foundCliIt == clients.cend())
			return CreateError("Error kicking client with name \"%s\" from channel; client with that name not found on server.", TStringToUTF8(clientNamePtr).c_str());
		clientToUse = *foundCliIt;
	}

	if (clientToUse->readonly())
	{
		return CreateError("Error kicking client \"%s\" (ID %hu) from channel \"%s\"; client is read-only.",
			clientToUse->name().c_str(), clientToUse->id(), selChannel->name().c_str());
	}

	// Check that channel contains client, and client contains channel; there may be inconsistency if client is currently leaving.
	{
		auto chCliReadLock = selChannel->lock.createReadLock();
		const auto & clientsOnChannel = selChannel->getclients();
		if (std::find(clientsOnChannel.cbegin(), clientsOnChannel.cend(), selClient) == clientsOnChannel.cend())
		{
			return CreateError("Error kicking client \"%s\" (ID %hu) from channel \"%s\"; client is not on the channel.",
				clientToUse->name().c_str(), clientToUse->id(), selChannel->name().c_str());
		}
	}
	{
		auto cliChReadLock = clientToUse->lock.createReadLock();
		const auto & channelsOnClient = clientToUse->getchannels();
		if (std::find(channelsOnClient.cbegin(), channelsOnClient.cend(), selChannel) == channelsOnClient.cend())
		{
			return CreateError("Error kicking client \"%s\" (ID %hu) from channel \"%s\"; client is not on the channel.",
				clientToUse->name().c_str(), clientToUse->id(), selChannel->name().c_str());
		}
	}

	// All checks passed; make it happen
	Srv.leavechannel_response(selChannel, clientToUse, std::string_view());
}
void Extension::LoopAllChannels()
{
	auto origSelChannel = selChannel;
	auto origSelClient = selClient;
	auto origLoopName = loopName;

	std::vector<decltype(origSelChannel)> serverChannelListDup;
	{
		auto serverReadLock = Srv.lock.createReadLock();
		serverChannelListDup = Srv.getchannels();
	}

	for (const auto& ch : serverChannelListDup)
	{
		selChannel = ch;
		selClient = nullptr;
		loopName = std::tstring_view();
		Runtime.GenerateEvent(5);
	}
	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = std::tstring_view();

	Runtime.GenerateEvent(45);
	loopName = origLoopName;
}
void Extension::LoopAllChannelsWithName(const TCHAR * passedLoopName)
{
	auto origSelChannel = selChannel;
	auto origSelClient = selClient;
	auto origLoopName = loopName;

	const std::tstring_view loopNameDup(passedLoopName);
	std::vector<decltype(origSelChannel)> serverChannelListDup;
	{
		auto serverReadLock = Srv.lock.createReadLock();
		serverChannelListDup = Srv.getchannels();
	}

	for (const auto& ch : serverChannelListDup)
	{
		selChannel = ch;
		selClient = nullptr;
		loopName = loopNameDup;
		Runtime.GenerateEvent(36);
	}
	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = loopNameDup;

	Runtime.GenerateEvent(41);
	loopName = origLoopName;
}
void Extension::Client_Disconnect()
{
	if (!selClient)
		return CreateError("Could not disconnect client: No client selected.");

	if (!selClient->readonly())
		selClient->disconnect();
}
void Extension::Client_SetLocalData(const TCHAR * key, const TCHAR * value)
{
	if (!selClient)
		return CreateError("Could not set client local data: No client selected.");
	// if (selClient->readonly())
	//	return CreateError("Could not set client local data: Client is read-only.");

	globals->SetLocalData(selClient, key, value);
}
void Extension::Client_JoinToChannel(const TCHAR * channelNamePtr)
{
	CreateError("Not implemented.");
}
void Extension::Client_LoopJoinedChannels()
{
	if (!selClient)
		return CreateError("Cannot loop client's joined channels: No client selected.");

	auto origSelClient = selClient;
	auto origSelChannel = selChannel;
	auto origLoopName = loopName;
	std::vector<decltype(selChannel)> joinedChannelListDup;
	{
		auto selClientReadLock = origSelClient->lock.createReadLock();
		joinedChannelListDup = origSelClient->getchannels();
	}

	for (const auto &joinedCh : joinedChannelListDup)
	{
		selChannel = joinedCh;
		selClient = origSelClient;
		loopName = std::tstring_view();
		Runtime.GenerateEvent(6);
	}

	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = std::tstring_view();

	Runtime.GenerateEvent(47);
	loopName = origLoopName;
}
void Extension::Client_LoopJoinedChannelsWithName(const TCHAR * passedLoopName)
{
	if (!selClient)
		return CreateError("Cannot loop client's joined channels: No client selected.");

	auto origSelClient = selClient;
	auto origSelChannel = selChannel;
	auto origLoopName = loopName;
	const std::tstring_view loopNameDup(passedLoopName);
	std::vector<decltype(selChannel)> joinedChannelListDup;
	{
		auto selClientReadLock = origSelClient->lock.createReadLock();
		joinedChannelListDup = origSelClient->getchannels();
	}

	for (const auto &joinedCh : joinedChannelListDup)
	{
		selChannel = joinedCh;
		selClient = origSelClient;
		loopName = loopNameDup;
		Runtime.GenerateEvent(37);
	}

	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = loopNameDup;

	Runtime.GenerateEvent(43);
	loopName = origLoopName;
}
void Extension::Client_SelectByName(const TCHAR * clientName)
{
	if (clientName[0] == _T('\0'))
		return CreateError("Select Client By Name was called with a blank name.");

	selClient = nullptr;
	{
		const std::string clientNameU8Simplified = TStringToUTF8Simplified(clientName);
		auto serverReadLock = Srv.lock.createReadLock();
		const auto &clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[&](const auto &cli) { return lw_sv_cmp(cli->nameSimplified(), clientNameU8Simplified); });
		if (foundCliIt == clients.cend())
			return CreateError("Client with name %s not found on server.", TStringToUTF8(clientName).c_str());
		selClient = *foundCliIt;
	}

	// If client is joined to originally selected channel, then keep that channel selected
	if (!selChannel)
		return;

	auto cliReadLock = selClient->lock.createReadLock();
	const auto & cliJoinChs = selClient->getchannels();
	if (std::find(cliJoinChs.cbegin(), cliJoinChs.cend(), selChannel) == cliJoinChs.cend())
		selChannel = nullptr;
}
void Extension::Client_SelectByID(int clientID)
{
	if (clientID < 0 || clientID >= 0xFFFF)
		return CreateError("Could not select client on channel, ID is below 0 or greater than 65535.");

	selClient = nullptr;
	{
		auto serverReadLock = Srv.lock.createReadLock();
		const auto &clients = Srv.getclients();
		auto foundCliIt =
			std::find_if(clients.cbegin(), clients.cend(),
				[=](const auto & cli) { return cli->id() == clientID; });
		if (foundCliIt == clients.cend())
			return CreateError("Client with ID %i not found on server.", clientID);
		selClient = *foundCliIt;
	}

	// If client is joined to originally selected channel, then keep that channel selected with new client
	if (!selChannel)
		return;

	auto cliReadLock = selClient->lock.createReadLock();
	const auto& cliJoinChs = selClient->getchannels();
	if (std::find(cliJoinChs.cbegin(), cliJoinChs.cend(), selChannel) == cliJoinChs.cend())
		selChannel = nullptr;
}
void Extension::Client_SelectSender()
{
	if (!threadData->senderClient)
		return CreateError("Cannot select sending client: No sending client variable available.");

	selClient = threadData->senderClient;
}
void Extension::Client_SelectReceiver()
{
	if (!threadData->receivingClient)
		return CreateError("Cannot select receiving client: No receiving client variable available.");

	selClient = threadData->receivingClient;
}
void Extension::LoopAllClients()
{
	auto origSelClient = selClient;
	auto origSelChannel = selChannel;
	auto origLoopName = loopName;

	// Let user write to clients if necessary by duping
	std::vector<decltype(selClient)> clientListDup;
	{
		auto serverReadLock = Srv.lock.createReadLock();
		clientListDup = Srv.getclients();
	}

	for (const auto & selectedClient : clientListDup)
	{
		selChannel = nullptr;
		selClient = selectedClient;
		loopName = std::tstring_view();
		Runtime.GenerateEvent(7);
	}

	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = std::tstring_view();
	Runtime.GenerateEvent(46);
	loopName = origLoopName;
}
void Extension::LoopAllClientsWithName(const TCHAR * passedLoopName)
{
	auto origSelClient = selClient;
	auto origSelChannel = selChannel;
	auto origLoopName = loopName;
	const std::tstring_view loopNameDup(passedLoopName);

	// Let user write to clients if necessary by duping
	std::vector<decltype(selClient)> clientListDup;
	{
		auto serverReadLock = Srv.lock.createReadLock();
		clientListDup = Srv.getclients();
	}

	for (const auto & selectedClient : clientListDup)
	{
		selChannel = nullptr;
		selClient = selectedClient;
		loopName = loopNameDup;
		Runtime.GenerateEvent(38);
	}

	selChannel = origSelChannel;
	selClient = origSelClient;
	loopName = loopNameDup;
	Runtime.GenerateEvent(42);

	loopName = origLoopName;
}
void Extension::SendTextToChannel(int subchannel, const TCHAR * textToSend)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Send Text to Channel was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selChannel)
		return CreateError("Send Text to Channel was called without a channel being selected.");
	if (selChannel->readonly())
		return CreateError("Send Text to Channel was called with a read-only channel, name %s.", selChannel->name().c_str());

	const std::string textToSendU8 = TStringToUTF8(textToSend);
	selChannel->send(subchannel, std::string_view(textToSendU8.c_str(), textToSendU8.size() + 1U), 0);
}
void Extension::SendTextToClient(int subchannel, const TCHAR * textToSend)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Send Text to Client was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selClient)
		return CreateError("Send Text to Client was called without a client being selected.");
	if (selClient->readonly())
		return CreateError("Send Text to Client was called with a read-only client ID %hu, name %s.", selClient->id(), selClient->name().c_str());

	const std::string textAsU8 = TStringToUTF8(textToSend);
	selClient->send(subchannel, std::string_view(textAsU8.c_str(), textAsU8.size() + 1U), 0);
}
void Extension::SendNumberToChannel(int subchannel, int numToSend)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Send Number to Channel was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selChannel)
		return CreateError("Send Number to Channel was called without a channel being selected.");
	if (selChannel->readonly())
		return CreateError("Send Number to Channel was called with a read-only channel, name %s.", selChannel->name().c_str());

	selChannel->send(subchannel, std::string_view((char *)&numToSend, sizeof(int)), 1);
}
void Extension::SendNumberToClient(int subchannel, int numToSend)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Send Number to Client was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selClient)
		return CreateError("Send Number to Client was called without a client being selected.");
	if (selClient->readonly())
		return CreateError("Send Number to Client was called with a read-only client ID %hu, name %s.", selClient->id(), selClient->name().c_str());

	selClient->send(subchannel, std::string_view((char *)&numToSend, sizeof(int)), 1);
}
void Extension::SendBinaryToChannel(int subchannel)
{
	if (subchannel > 255 || subchannel < 0)
		CreateError("Send Binary to Channel was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	else if (!selChannel)
		CreateError("Send Binary to Channel was called without a channel being selected.");
	else if (selChannel->readonly())
		CreateError("Send Binary to Channel was called with a read-only channel, name %s.", selChannel->name().c_str());
	else
		selChannel->send(subchannel, std::string_view(SendMsg, SendMsgSize), 2);

	if (AutomaticallyClearBinary)
		SendMsg_Clear();
}
void Extension::SendBinaryToClient(int subchannel)
{
	if (subchannel > 255 || subchannel < 0)
		CreateError("Send Binary to Client was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	else if (!selClient)
		CreateError("Send Binary to Client was called without a client being selected.");
	else if (selClient->readonly())
		CreateError("Send Binary to Client was called with a read-only client: ID %hu, name %s.", selClient->id(), selClient->name().c_str());
	else
		selClient->send(subchannel, std::string_view(SendMsg, SendMsgSize), 2);

	if (AutomaticallyClearBinary)
		SendMsg_Clear();
}
void Extension::BlastTextToChannel(int subchannel, const TCHAR * textToBlast)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Blast Text to Channel was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selChannel)
		return CreateError("Blast Text to Channel was called without a channel being selected.");
	if (selChannel->readonly())
		return CreateError("Blast Text to Channel was called with a read-only channel, name %s.", selChannel->name().c_str());

	const std::string textAsU8 = TStringToUTF8(textToBlast);
	selChannel->blast(subchannel, std::string_view(textAsU8.c_str(), textAsU8.size() + 1U), 0);
}
void Extension::BlastTextToClient(int subchannel, const TCHAR * textToBlast)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Blast Text to Client was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selClient)
		return CreateError("Blast Text to Client was called without a client being selected.");
	if (selClient->readonly())
		return CreateError("Blast Text to Client was called with a read-only client: ID %hu, name %s.", selClient->id(), selClient->name().c_str());

	const std::string textAsU8 = TStringToUTF8(textToBlast);
	selClient->blast(subchannel, std::string_view(textAsU8.c_str(), textAsU8.size() + 1U), 0);
}
void Extension::BlastNumberToChannel(int subchannel, int numToBlast)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Blast Number to Channel was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selChannel)
		return CreateError("Blast Number to Channel was called without a channel being selected.");
	if (selChannel->readonly())
		return CreateError("Blast Number to Channel was called with a read-only channel, name %s.", selChannel->name().c_str());

	selChannel->blast(subchannel, std::string_view((char *)&numToBlast, sizeof(int)), 1);
}
void Extension::BlastNumberToClient(int subchannel, int numToBlast)
{
	if (subchannel > 255 || subchannel < 0)
		return CreateError("Blast Number to Client was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	if (!selClient)
		return CreateError("Blast Number to Client was called without a client being selected.");
	if (selClient->readonly())
		return CreateError("Blast Number to Client was called with a read-only client: ID %hu, name %s.", selClient->id(), selClient->name().c_str());

	selClient->blast(subchannel, std::string_view((char *)&numToBlast, sizeof(int)), 1);
}
void Extension::BlastBinaryToChannel(int subchannel)
{
	if (subchannel > 255 || subchannel < 0)
		CreateError("Blast Binary to Channel was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	else if (!selChannel)
		CreateError("Blast Binary to Channel was called without a channel being selected.");
	else if (selChannel->readonly())
		CreateError("Blast Binary to Channel was called with a read-only channel, name %s.", selChannel->name().c_str());
	else
		selChannel->blast(subchannel, std::string_view(SendMsg, SendMsgSize), 2);

	if (AutomaticallyClearBinary)
		SendMsg_Clear();
}
void Extension::BlastBinaryToClient(int subchannel)
{
	if (subchannel > 255 || subchannel < 0)
		CreateError("Blast Binary to Client was called with an invalid subchannel %i; it must be between 0 and 255.", subchannel);
	else if (!selClient)
		CreateError("Blast Binary to Client was called without a client being selected.");
	else if (selClient->readonly())
		CreateError("Blast Binary to Client was called with a read-only client: ID %hu, name %s.", selClient->id(), selClient->name().c_str());
	else
		selClient->blast(subchannel, std::string_view(SendMsg, SendMsgSize), 2);

	if (AutomaticallyClearBinary)
		SendMsg_Clear();
}
void Extension::SendMsg_AddASCIIByte(const TCHAR * byte)
{
	const std::string u8Str(TStringToUTF8(byte));
	if (u8Str.size() != 1)
		return CreateError("Adding ASCII character to binary failed: byte \"%s\" supplied was part of a string, not a single byte.", u8Str.c_str());

	// ANSI byte, not ASCII; or not displayable, so probably a corrupt string.
	if (reinterpret_cast<const std::uint8_t &>(u8Str[0]) > 127 || !std::isprint(u8Str[0]))
		return CreateError("Adding ASCII character to binary failed: byte \"%u\" was not a valid ASCII character.", (unsigned int) reinterpret_cast<const std::uint8_t &>(u8Str[0]));

	SendMsg_Sub_AddData(u8Str.c_str(), sizeof(char));
}
void Extension::SendMsg_AddByteInt(int byte)
{
	if (byte > UINT8_MAX || byte < INT8_MIN)
	{
		return CreateError("Adding byte to binary (as int) failed: the supplied number %i will not fit in range "
			"%i to %i (signed byte) or range 0 to %i (unsigned byte).", byte, INT8_MIN, INT8_MAX, UINT8_MAX);
	}

	SendMsg_Sub_AddData(&byte, sizeof(char));
}
void Extension::SendMsg_AddShort(int _short)
{
	if (_short > UINT16_MAX || _short < INT16_MIN)
	{
		return CreateError("Adding short to binary failed: the supplied number %i will not fit in range "
			"%i to %i (signed short) or range 0 to %i (unsigned short).", _short, INT16_MIN, INT16_MAX, UINT16_MAX);
	}

	SendMsg_Sub_AddData(&_short, sizeof(short));
}
void Extension::SendMsg_AddInt(int _int)
{
	SendMsg_Sub_AddData(&_int, 4);
}
void Extension::SendMsg_AddFloat(float _float)
{
	SendMsg_Sub_AddData(&_float, 4);
}
void Extension::SendMsg_AddStringWithoutNull(const TCHAR * string)
{
	const std::string u8String = TStringToUTF8(string);
	SendMsg_Sub_AddData(u8String.c_str(), u8String.size());
}
void Extension::SendMsg_AddString(const TCHAR * string)
{
	const std::string u8Str = TStringToUTF8(string);
	SendMsg_Sub_AddData(u8Str.c_str(), u8Str.size() + 1U);
}
void Extension::SendMsg_AddBinaryFromAddress(unsigned int address, int size)
{
	// Address is checked in SendMsg_Sub_AddData()
	if (size < 0)
		return CreateError("Add binary from address failed: Size %i less than 0.", size);

	SendMsg_Sub_AddData((void *)(long)address, size);
}
void Extension::SendMsg_AddFileToBinary(const TCHAR * filename)
{
	if (filename[0] == _T('\0'))
		return CreateError("Cannot add file to send binary; filename \"\" is invalid.");

	// Open and deny other programs write privileges
	FILE * file = _tfsopen(filename, _T("rb"), _SH_DENYWR);
	if (!file)
	{
		ErrNoToErrText();
		return CreateError("Cannot add file \"%s\" to send binary, error %i (%hs) occurred with opening the file."
			" The send binary has not been modified.", TStringToUTF8(filename).c_str(), errno, errtext);
	}

	// Jump to end
	fseek(file, 0, SEEK_END);

	// Read current position as file size
	long filesize = ftell(file);

	// Go back to start
	fseek(file, 0, SEEK_SET);

	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(filesize);
	size_t amountRead;
	if ((amountRead = fread_s(buffer.get(), filesize, 1U, filesize, file)) != filesize)
	{
		CreateError("Couldn't read file \"%s\" into binary to send; couldn't reserve enough memory "
			"to add file into message. The send binary has not been modified.", TStringToUTF8(filename).c_str());
	}
	else
		SendMsg_Sub_AddData(buffer.get(), amountRead);

	fclose(file);
}
void Extension::SendMsg_Resize(int newSize)
{
	if (newSize < 0)
		return CreateError("Cannot resize binary to send: new size %u bytes is negative.", newSize);

	char * NewMsg = (char *)realloc(SendMsg, newSize);
	if (!NewMsg)
	{
		return CreateError("Cannot resize binary to send: reallocation of memory into %u bytes failed.\r\n"
			"Binary to send has not been modified.", newSize);
	}
	// Clear new bytes to 0
	memset(NewMsg + SendMsgSize, 0, newSize - SendMsgSize);

	SendMsg = NewMsg;
	SendMsgSize = newSize;
}
void Extension::SendMsg_CompressBinary()
{
	if (SendMsgSize <= 0)
		return CreateError("Cannot compress send binary; binary is empty.");

	z_stream strm = {};
	int ret = deflateInit(&strm, 9); // 9 is maximum compression level
	if (ret)
		return CreateError("Compressing send binary failed, zlib error %i \"%hs\" occurred with initiating compression.", ret, (strm.msg ? strm.msg : ""));

	// 4: precursor lw_ui32 with uncompressed size, required by Relay
	// 256: if compression results in larger message, it shouldn't be *that* much larger.

	std::uint8_t * output_buffer = (std::uint8_t *)malloc(4 + SendMsgSize + 256);
	if (!output_buffer)
	{
		CreateError("Compressing send binary failed, couldn't allocate enough memory. Desired %zu bytes.",
			(size_t)4 + SendMsgSize + 256);
		deflateEnd(&strm);
		return;
	}

	// Store size as precursor - required by Relay
	*(lw_ui32 *)output_buffer = SendMsgSize;

	strm.next_in = (std::uint8_t *)SendMsg;
	strm.avail_in = SendMsgSize;

	// Allocate memory for compression
	strm.avail_out = SendMsgSize - 4;
	strm.next_out = output_buffer + 4;

	ret = deflate(&strm, Z_FINISH);
	if (ret != Z_STREAM_END)
	{
		free(output_buffer);
		CreateError("Compressing send binary failed, zlib compression call returned error %u \"%hs\".",
			ret, (strm.msg ? strm.msg : ""));
		deflateEnd(&strm);
		return;
	}

	deflateEnd(&strm);

	char * output_bufferResize = (char *)realloc(output_buffer, 4 + strm.total_out);
	if (!output_bufferResize)
		return CreateError("Compressing send binary failed, reallocating memory to remove excess space after compression failed.");

	free(SendMsg);

	SendMsg = (char *)output_bufferResize;
	SendMsgSize = 4 + strm.total_out;
}
void Extension::SendMsg_Clear()
{
	free(SendMsg);
	SendMsg = NULL;
	SendMsgSize = 0;
}
void Extension::RecvMsg_DecompressBinary()
{
	if (threadData->receivedMsg.content.size() <= 4)
	{
		return CreateError("Cannot decompress received binary; message is %u bytes and too small to be a valid compressed message.",
			threadData->receivedMsg.content.size());
	}

	z_stream strm = { };
	int ret = inflateInit(&strm);
	if (ret)
	{
		return CreateError("Compressing send binary failed, zlib error %i \"%hs\" occurred with initiating decompression.",
			ret, (strm.msg ? strm.msg : ""));
	}

	// Lacewing provides a precursor to the compressed data, with uncompressed size.
	lw_ui32 expectedUncompressedSize = *(lw_ui32 *)threadData->receivedMsg.content.data();
	const std::string_view inputData(threadData->receivedMsg.content.data() + sizeof(lw_ui32), threadData->receivedMsg.content.size() - sizeof(lw_ui32));

	unsigned char * output_buffer = (unsigned char *)malloc(expectedUncompressedSize);
	if (!output_buffer)
	{
		inflateEnd(&strm);
		return CreateError("Decompression failed; couldn't allocate enough memory. Desired %u bytes.", expectedUncompressedSize);
	}

	strm.next_in = (unsigned char *)inputData.data();
	strm.avail_in = inputData.size();
	strm.avail_out = expectedUncompressedSize;
	strm.next_out = output_buffer;
	ret = inflate(&strm, Z_FINISH);
	if (ret < Z_OK)
	{
		free(output_buffer);
		CreateError("Decompression failed; zlib decompression call returned error %i \"%hs\".",
			ret, (strm.msg ? strm.msg : ""));
		inflateEnd(&strm);
		return;
	}

	inflateEnd(&strm);

	// Used to assign all exts in a questionable way, but threadData is now std::shared_ptr, so no need.
	threadData->receivedMsg.content.assign((char *)output_buffer, expectedUncompressedSize);
	threadData->receivedMsg.cursor = 0;

	free(output_buffer); // .assign() copies the memory
}
void Extension::RecvMsg_MoveCursor(int position)
{
	if (position < 0)
		return CreateError("Cannot move cursor; Position less than 0.");
	if (threadData->receivedMsg.content.size() - position <= 0)
	{
		return CreateError("Cannot move cursor to index %i; message is too small. Valid cursor index range is 0 to %i.",
			position, max(threadData->receivedMsg.content.size() - 1, 0));
	}

	threadData->receivedMsg.cursor = position;
}
void Extension::RecvMsg_SaveToFile(int passedPosition, int passedSize, const TCHAR * filename)
{
	if (passedPosition < 0)
		return CreateError("Cannot save received binary; position %i is less than 0.", passedPosition);
	if (passedSize <= 0)
		return CreateError("Cannot save received binary; size of %i is equal or less than 0.", passedSize);
	if (filename[0] == _T('\0'))
		return CreateError("Cannot save received binary; filename \"\" is invalid.");

	unsigned int position = (unsigned int)passedPosition;
	unsigned int size = (unsigned int)passedSize;

	if (position + size > threadData->receivedMsg.content.size())
	{
		return CreateError("Cannot save received binary to file \"%s\"; message doesn't have %i"
			" bytes from position %u onwards, it only has %u bytes.",
			TStringToUTF8(filename).c_str(), size, position, threadData->receivedMsg.content.size() - position);
	}
	FILE * File = _tfsopen(filename, _T("wb"), SH_DENYWR);
	if (!File)
	{
		ErrNoToErrText();
		CreateError("Cannot save received binary to file \"%s\", error %i \"%s\""
			" occurred with opening the file.", TStringToUTF8(filename).c_str(), errno, errtext);
		return;
	}

	size_t amountWritten;
	if ((amountWritten = fwrite(threadData->receivedMsg.content.data() + position, 1, size, File)) != size)
	{
		ErrNoToErrText();
		CreateError("Cannot save received binary to file \"%s\", error %i \"%s\""
			" occurred with writing the file. Wrote %zu bytes total.", TStringToUTF8(filename).c_str(), errno, errtext, amountWritten);
		fclose(File);
		return;
	}

	if (fclose(File))
	{
		ErrNoToErrText();
		CreateError("Cannot save received binary to file \"%s\", error %i \"%s\""
			" occurred with writing the end of the file.", TStringToUTF8(filename).c_str(), errno, errtext);
	}
}
void Extension::RecvMsg_AppendToFile(int passedPosition, int passedSize, const TCHAR * filename)
{
	if (passedPosition < 0)
		return CreateError("Cannot append received binary; position %i is less than 0.", passedPosition);
	if (passedSize <= 0)
		return CreateError("Cannot append received binary; size of %i is equal or less than 0.", passedSize);
	if (filename[0] == '\0')
		return CreateError("Cannot append received binary; filename \"\" is invalid.");

	size_t position = (size_t)passedPosition;
	size_t size = (size_t)passedSize;
	if (position + size > threadData->receivedMsg.content.size())
	{
		return CreateError("Cannot append received binary to file \"%s\"; message doesn't have %i"
			" bytes from position %u onwards, it only has %u bytes.",
			TStringToUTF8(filename).c_str(), size, position, threadData->receivedMsg.content.size() - position);
	}
	FILE * File = _tfsopen(filename, _T("ab"), SH_DENYWR);
	if (!File)
	{
		ErrNoToErrText();
		CreateError("Cannot append received binary to file \"%s\", error %i \"%s\""
			" occurred with opening the file.", TStringToUTF8(filename).c_str(), errno, errtext);
		return;
	}

	size_t amountWritten;
	if ((amountWritten = fwrite(threadData->receivedMsg.content.data() + position, 1, size, File)) != size)
	{
		ErrNoToErrText();
		CreateError("Cannot append received binary to file \"%s\", error %i \"%s\""
			" occurred with writing the file. Wrote %zu bytes total.", TStringToUTF8(filename).c_str(), errno, errtext, amountWritten);
		fclose(File);
		return;
	}

	if (fclose(File))
	{
		ErrNoToErrText();
		CreateError("Cannot append received binary to file \"%s\", error %i \"%s\""
			" occurred with writing the end of the file.", TStringToUTF8(filename).c_str(), errno, errtext);
	}
}
