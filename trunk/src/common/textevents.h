/* this file is auto generated, edit textevents.in instead! */

const struct text_event te[] = {

{"Add Notify", pevt_generic_nick_help, 1, 
N_("%C0%B%171Notify%187%B$tNow watching nick $1.%O")},

{"Ban List", pevt_banlist_help, 4, 
N_("%C0%B%171Ban List%187%B$tBan for channel $1: $2 $3 $4")},

{"Banned", pevt_generic_channel_help, 1, 
N_("%C5%B%171Error%187%B%O$tYou are banned from channel $1.%O")},

{"Beep", pevt_generic_none_help, 0, 
""},

{"Change Nick", pevt_changenick_help, 2, 
N_("%C0%B%171Nick%187%B$t$1 is now known as $2%O")},

{"Channel Action", pevt_chanaction_help, 2, 
N_("%C11*%O$t$1 $2%O")},

{"Channel Action Hilight", pevt_chanaction_help, 2, 
N_("%C16*$t$1 $2%O")},

{"Channel Ban", pevt_chanban_help, 2, 
N_("%C0%B%171Ban%187%B$t$1 sets ban on \"$2\".%O")},

{"Channel Creation", pevt_chandate_help, 2, 
N_("%C0%B%171Mode%187%B$tChannel $1 was created on $2.%O")},

{"Channel DeHalfOp", pevt_chandehop_help, 2, 
N_("%C0%B%171DEHalf-OP%187%B$t$1 removes channel half-operator status from $2.%O")},

{"Channel DeOp", pevt_chandeop_help, 2, 
N_("%C0%B%171DEOP%187%B$t$1 removes channel operator status from $2.%O")},

{"Channel DeVoice", pevt_chandevoice_help, 2, 
N_("%C0%B%171Voice%187%B$t$1 removes voice from $2.%O")},

{"Channel Exempt", pevt_chanexempt_help, 2, 
N_("%C0%B%171Exempt%187%B$t$1 sets exempt on $2.%O")},

{"Channel Half-Operator", pevt_chanhop_help, 2, 
N_("%C0%B%171Half-OP%187%B$t$1 gives channel half-operator status to $2.%O")},

{"Channel INVITE", pevt_chaninvite_help, 2, 
N_("%C0%B%171Invite%187%B$t$1 sets invite on $2.%O")},

{"Channel List", pevt_generic_none_help, 0, 
N_("%UChannel          Users   Topic%O")},

{"Channel Message", pevt_chanmsg_help, 4, 
N_("<$1>$t$2%O")},

{"Channel Mode Generic", pevt_chanmodegen_help, 4, 
N_("%C0%B%171Mode%187%B$t$1 sets mode $2$3 on $4.%O")},

{"Channel Modes", pevt_chanmodes_help, 2, 
N_("%C0%B%171Mode%187%B$tChannel modes for $1: $2%O")},

{"Channel Msg Hilight", pevt_chanmsg_help, 4, 
N_("%C16<$1>$t$2%O")},

{"Channel Notice", pevt_channotice_help, 3, 
N_("%C16-$1-%O$t[to $2]%O $3%O")},

{"Channel Operator", pevt_chanop_help, 2, 
N_("%C0%B%171OP%187%B$t$1 gives channel operator status to $2%O")},

{"Channel Remove Exempt", pevt_chanrmexempt_help, 2, 
N_("%C0%B%171Exempt%187%B$t$1 removes exempt on $2%O")},

{"Channel Remove Invite", pevt_chanrminvite_help, 2, 
N_("%C0%B%171Invite%187%B$t$1 removes invite on $2%O")},

{"Channel Remove Keyword", pevt_chanrmkey_help, 1, 
N_("%C0%B%171Mode%187%B$t$1 chages channel modes to no keyword.%O")},

{"Channel Remove Limit", pevt_chanrmlimit_help, 1, 
N_("%C0%B%171Mode%187%B$t$1 changes channel mode to no user limit.%O")},

{"Channel Set Key", pevt_chansetkey_help, 2, 
N_("%C0%B%171Mode%187%B$t$1 sets channel keyword to \"$2\".%O")},

{"Channel Set Limit", pevt_chansetlimit_help, 2, 
N_("%C0%B%171Mode%187%B$t$1 changes channel modes limited to $2 user(s).%O")},

{"Channel UnBan", pevt_chanunban_help, 2, 
N_("%C0%B%171UnBan%187%B$t$1 removes ban on \"$2\"%O")},

{"Channel Voice", pevt_chanvoice_help, 2, 
N_("%C0%B%171Voice%187%B$t$1 gives voice to $2%O")},

{"Connected", pevt_generic_none_help, 0, 
N_("%C0%B%171Connect%187%B$tConnected. Now doing login...%O")},

{"Connecting", pevt_connect_help, 3, 
N_("%C0%B%171Connect%187%B$tAttempting connection to host to $1 ($2) on port $3...%O")},

{"Connection Failed", pevt_connfail_help, 1, 
N_("%C5%B%171Net-Error%187%B%O$tConnection failed. Error: $1%O")},

{"CTCP Generic", pevt_ctcpgen_help, 2, 
N_("%C4%B%171CTCP%187%B$tReceived CTCP \"$1\" from $2%O")},

{"CTCP Generic to Channel", pevt_ctcpgenc_help, 3, 
N_("%C4%B%171CTCP%187%B$tReceived CTCP \"$1\" from $2 (to $3)")},

{"CTCP Send", pevt_ctcpsend_help, 2, 
N_("%C4%B%171CTCP%187%B$tSending CTCP \"$2\" to $1.%O")},

{"CTCP Sound", pevt_ctcpsnd_help, 2, 
N_("%C4%B%171Sound%187%B$t$2 [playing $1]%O")},

{"DCC CHAT Abort", pevt_dccchatabort_help, 1, 
N_("%C8%B%171DCC%187%B$tAborted DCC CHAT to $1.%O")},

{"DCC CHAT Connect", pevt_dccchatcon_help, 2, 
N_("%C8%B%171DCC%187%B$tEstablished DCC CHAT connection to $1%O")},

{"DCC CHAT Failed", pevt_dccchaterr_help, 4, 
N_("%C8%B%171DCC%187%B$tChat failed. Connection to $1 ($2:$3) lost.%O")},

{"DCC CHAT Offer", pevt_generic_nick_help, 1, 
N_("%C8%B%171DCC%187%B$tGot DCC CHAT request from $1.%O")},

{"DCC CHAT Offering", pevt_generic_nick_help, 1, 
N_("%C8%B%171DCC%187%B$tSending DCC CHAT offer to $1.%O")},

{"DCC CHAT Reoffer", pevt_generic_nick_help, 1, 
N_("%C8%B%171DCC%187%B$tAlready offering CHAT to $1.%O")},

{"DCC Conection Failed", pevt_dccconfail_help, 3, 
N_("%C8%B%171DCC%187%B$tDCC $1 connection attempt to $2 failed (err=$3).%O")},

{"DCC Generic Offer", pevt_dccgenericoffer_help, 2, 
N_("%C8%B%171DCC%187%B$tReceived \"$1\" from $2.%O")},

{"DCC Header", pevt_generic_none_help, 0, 
N_("%C8,2 Type  To/From    Status  Size    Pos     File         %O")},

{"DCC Malformed", pevt_malformed_help, 2, 
N_("%C8%B%171DCC%187%B$tGot malformed DCC request from $1.%O%010%C8%B%171DCC%187%B$tContents of packet: \"$2\".%O")},

{"DCC Offer", pevt_dccoffer_help, 3, 
N_("%C8%B%171DCC%187%B$tOffering $1 to $2%O")},

{"DCC Offer Not Valid", pevt_generic_none_help, 0, 
N_("%C8%B%171DCC%187%B$tNo such DCC offer.%O")},

{"DCC RECV Abort", pevt_dccfileabort_help, 2, 
N_("%C8%B%171DCC%187%B$tAborted DCC RECV $2 from $1%O")},

{"DCC RECV Complete", pevt_dccrecvcomp_help, 4, 
N_("%C8%B%171DCC%187%B$tReceive of file $1 from $3 completed, $4 cps.%O")},

{"DCC RECV Connect", pevt_dcccon_help, 3, 
N_("%C8%B%171DCC%187%B$tEstablished DCC RECV connection for $3 from $1 ($2)%O")},

{"DCC RECV Failed", pevt_dccrecverr_help, 4, 
N_("%C8%B%171DCC%187%B$tReceive $1 failed. Connection to $3 lost.%O")},

{"DCC RECV File Open Error", pevt_generic_file_help, 2, 
N_("%C8%B%171DCC%187%B$tReceive Write error ($1).%O")},

{"DCC Rename", pevt_dccrename_help, 2, 
N_("%C8%B%171DCC%187%B$t A file named $1 already exists, saving it as $2 instead.%O")},

{"DCC RESUME Request", pevt_dccresumeoffer_help, 3, 
N_("%C8%B%171DCC%187%B$tGot DCC RESUME request for SEND of file $2, position $3, from $1.%O")},

{"DCC SEND Abort", pevt_dccfileabort_help, 2, 
N_("%C8%B%171DCC%187%B$tDCC SEND of file $2 to $1 aborted.%O")},

{"DCC SEND Complete", pevt_dccsendcomp_help, 3, 
N_("%C8%B%171DCC%187%B$tDCC SEND of file $1 to $2 complete, $3 cps.%O")},

{"DCC SEND Connect", pevt_dcccon_help, 3, 
N_("%C8%B%171DCC%187%B$tEstablished DCC SEND connection for $3 to $1 ($2)%O")},

{"DCC SEND Failed", pevt_dccsendfail_help, 3, 
N_("%C8%B%171DCC%187%B$tSending $1 to $2 failed. Connection lost.%O")},

{"DCC SEND Offer", pevt_dccsendoffer_help, 4, 
N_("%C8%B%171DCC%187%B$tGot DCC SEND request from $1: filename \"$2\", $3 bytes.%O")},

{"DCC Stall", pevt_dccstall_help, 3, 
N_("%C8%B%171DCC%187%B$tDCC $1 $2 to $3 is stalled - aborting.%O")},

{"DCC Timeout", pevt_dccstall_help, 3, 
N_("%C8%B%171DCC%187%B$tDCC $1 $2 to $3 timed out - aborting.%O")},

{"Delete Notify", pevt_generic_nick_help, 1, 
N_("%C0%B%171Notify%187%B$tNick $1 is no longer watched.%O")},

{"Disconnected", pevt_discon_help, 1, 
N_("%C0%B%171Connect%187%B$tDisconnected.%O")},

{"Found IP", pevt_foundip_help, 1, 
N_("%C0%B%171IP%187%B$tFound your IP: $1%O")},

{"Generic Message", pevt_genmsg_help, 2, 
N_("$1$t$2")},

{"Ignore Add", pevt_ignoreaddremove_help, 1, 
N_("%C0%B%171Ignore%187%B$tNow ignoring \"$1\".%O")},

{"Ignore Changed", pevt_ignoreaddremove_help, 1, 
N_("%C0%B%171Ignore%187%B$tIgnore on \"$1\" changed.%O")},

{"Ignore Footer", pevt_generic_none_help, 0, 
N_("%C0%B%171Ignore%187%B$tEnd of /ignore list.%O")},

{"Ignore Header", pevt_generic_none_help, 0, 
N_("%C0%B%171Ignore%187%B%O$t%C08,02 Hostmask                  PRIV NOTI CHAN CTCP INVI UNIG %O")},

{"Ignore Remove", pevt_ignoreaddremove_help, 1, 
N_("%C0%B%171Ignore%187%B$tIgnore entry \"$1\" removed.%O")},

{"Ignorelist Empty", pevt_generic_none_help, 0, 
N_("%C0%B%171Ignore%187%B$tIgnore list is empty.%O")},

{"Invite", pevt_generic_channel_help, 1, 
N_("%C5%B%171Error%187%B%O$tYou can't join $1; this channel is invite Only.%O")},

{"Invited", pevt_invited_help, 3, 
N_("%C0%B%171Invite%187%B$tYou have been invited to channel $1 by $2 ($3)%O")},

{"Join", pevt_join_help, 3, 
N_("%C0%B-->%B$t$1 ($3) has joined channel $2%O")},

{"Keyword", pevt_generic_channel_help, 1, 
N_("%C5%B%171Error%187%B%O$tChannel $1 is keyword protected; you must specify the correct keyword.%O")},

{"Kick", pevt_kick_help, 4, 
N_("%C0%B%171Kick%187%B$t$2 has been kicked out of channel $3 by $1: $4%O")},

{"Killed", pevt_kill_help, 2, 
N_("%C0%B%171KILL%187%B$tYou have been killed by $1: $2%O")},

{"Message Send", pevt_ctcpsend_help, 2, 
N_("%B>$1<%B$t$2%O")},

{"Motd", pevt_servertext_help, 1, 
N_("%C0%B%171MOTD%187%B$t$1%O")},

{"MOTD Skipped", pevt_generic_none_help, 0, 
N_("%C0%B%171MOTD%187%B$t(MOTD Skipped)%O")},

{"Nick Clash", pevt_nickclash_help, 2, 
N_("%C0%B%171Nick%187%B$tNickname $1 is already in use; retrying with $2...%O")},

{"Nick Failed", pevt_generic_none_help, 0, 
N_("%C0%B%171Nick%187%B$tNickname is already in use.%O")},

{"No DCC", pevt_generic_none_help, 0, 
N_("%C8%B%171DCC%187%B$tNo such DCC.%O")},

{"No Running Process", pevt_generic_none_help, 0, 
N_("%C0%B%171Process%187%B$tNo process is currently running.%O")},

{"Notice", pevt_notice_help, 2, 
N_("-%B$1%B-$t$2%O")},

{"Notice Send", pevt_ctcpsend_help, 2, 
N_("}$1{$t$2")},

{"Notify Empty", pevt_generic_none_help, 0, 
N_("%C0%B%171Notify%187%B$tNotify list is empty.%O")},

{"Notify Header", pevt_generic_none_help, 0, 
N_("%C08,02 -- Notify List --------------- %O")},

{"Notify Number", pevt_notifynumber_help, 1, 
N_("%C0%B%171Notify%187%B$t$1 users on notify list.%O")},

{"Notify Offline", pevt_generic_nick_help, 2, 
N_("%C0%B%171Notify%187%B$tNick $1 ($2) is gone.%O")},

{"Notify Online", pevt_generic_nick_help, 2, 
N_("%C0%B%171Notify%187%B$tDetected Nick $1 ($2).%O")},

{"Open Dialog", pevt_generic_none_help, 0, 
""},

{"Part", pevt_part_help, 3, 
N_("%C0%B<--%B$t$1 ($2) has left channel $3%O")},

{"Part with Reason", pevt_partreason_help, 4, 
N_("%C0%B<--%B$t$1 ($2) has left channel $3 ($4).%O")},

{"Ping Reply", pevt_pingrep_help, 2, 
N_("%C4%B%171Pong%187%B$tGot PING response from $1: $2s%O")},

{"Ping Timeout", pevt_pingtimeout_help, 1, 
N_("%C5%B%171Error%187%B%O$tNo ping reply for $1 seconds, disconnecting.")},

{"Private Message", pevt_privmsg_help, 3, 
N_("%B*$1*%B$t$2%O")},

{"Private Message to Dialog", pevt_dprivmsg_help, 3, 
N_("%C16*$1*$t$2%O")},

{"Process Already Running", pevt_generic_none_help, 0, 
N_("%C0%B%171Notify%187%B$tA process is already running.%O")},

{"Quit", pevt_quit_help, 3, 
N_("%C0%B%171Quit%187%B$t$1 has signed off ($2)%O")},

{"Raw Modes", pevt_rawmodes_help, 2, 
N_("%C0%B%171Mode%187%B$t$1 sets mode(s): $2%O")},

{"Receive Wallops", pevt_dprivmsg_help, 2, 
N_("%C0%B%171Wallops%187%B$t[$1] $2%O")},

{"Resolving User", pevt_resolvinguser_help, 2, 
N_("%C0%B%171DNS%187%B$tLooking up IP number for $1 ($2)...%O")},

{"Server Connected", pevt_generic_none_help, 0, 
N_("%C0%B%171Connect%187%B$tConnected. Now doing login...%O")},

{"Server Error", pevt_servererror_help, 1, 
N_("%C5%B%171Error%187%B%O$t$1%O")},

{"Server Lookup", pevt_serverlookup_help, 1, 
N_("%C0%B%171Connect%187%B$tLooking up host \"$1\"...%O")},

{"Server Notice", pevt_servertext_help, 2, 
N_("%C0%B%171Server%187%B$t[$2] $1%O")},

{"Server Text", pevt_servertext_help, 3, 
N_("%C0%B%171Server%187%B$t$1%O")},

{"Stop Connection", pevt_sconnect_help, 1, 
N_("%C0%B%171Connect%187%B$tStopped previous connection attempt (pid=$1)%O")},

{"Topic", pevt_topic_help, 2, 
N_("%C0%B%171Topic%187%B$tTopic for $1 is $2%O")},

{"Topic Creation", pevt_topicdate_help, 3, 
N_("%C0%B%171Topic%187%B$tTopic for channel $1 has been set by $2 on $3%O")},

{"Topic Change", pevt_newtopic_help, 3, 
N_("%C0%B%171Topic%187%B$t$1 sets topic to \"$2\"%O")},

{"Unknown Host", pevt_generic_none_help, 0, 
N_("%C5%B%171Error%187%B%O$tUnknown host. Maybe you misspelled it?%O")},

{"User Limit", pevt_generic_channel_help, 1, 
N_("%C5%B%171Error%187%B%O$tYou can't join $1; user limit reached.%O")},

{"Users On Channel", pevt_usersonchan_help, 2, 
N_("%C0%B%171Names%187%B$tUsers on $1: $2%O")},

{"WhoIs Away Line", pevt_whois5_help, 2, 
N_("%C0%B%171Whois%187%B$t$1 is away: $2%O")},

{"WhoIs Channel/Oper Line", pevt_whois2_help, 2, 
N_("%C0%B%171Whois%187%B$t$1 $2%O")},

{"WhoIs End", pevt_whois6_help, 1, 
N_("%C0%B%171Whois%187%B$tEnd of WHOIS list.%O")},

{"WhoIs Identified", pevt_whoisid_help, 2, 
N_("%C0%B%171Whois%187%B$t%B[%B$1%B]%B $2")},

{"WhoIs Authenticated", pevt_whoisauth_help, 3, 
N_("%C0%B%171Whois%187%B$t%B[%B$1%B]%B $2 $3")},

{"WhoIs Real Host", pevt_whoisrealhost_help, 4, 
N_("%C0%B%171Whois%187%B$t%B[%B$1%B]%B real user@host $2, real IP $3")},

{"WhoIs Idle Line", pevt_whois4_help, 2, 
N_("%C0%B%171Whois%187%B$tIdle: $2%O")},

{"WhoIs Idle Line with Signon", pevt_whois4t_help, 3, 
N_("%C0%B%171Whois%187%B$tIdle: $2, Connected since: $3%O")},

{"WhoIs Name Line", pevt_whois1_help, 4, 
N_("%C0%B%171Whois%187%B$tUser: $1 ($2@$3): $4%O")},

{"WhoIs Server Line", pevt_whois3_help, 2, 
N_("%C0%B%171Whois%187%B$tServer: $2%O")},

{"You Join", pevt_join_help, 3, 
N_("%C0%B-->%B$tYou have joined channel $2%O")},

{"You Part", pevt_part_help, 3, 
N_("%C0%B<--%B$tYou have left channel $3.")},

{"You Part with Reason", pevt_partreason_help, 4, 
N_("%C0%B<--%B$tYou have left channel $3 ($4).%O")},

{"You Kicked", pevt_ukick_help, 4, 
N_("%C0%B%171Kick%187%B$t$1 has been kicked out of channel $2 by $3: $4%O")},

{"Your Invitation", pevt_uinvite_help, 3, 
N_("%C0%B%171Invite%187%B$tInviting $1 to channel $2.%O")},

{"Your Message", pevt_chanmsg_help, 4, 
N_("<$1>$t$2%O")},

{"Your Nick Changing", pevt_uchangenick_help, 2, 
N_("%C0%B%171Nick%187%B$tYou are now known as $2%O")},

{"RPong Message", pevt_rpong_help, 4, 
N_("%C0%B%171RPong%187%B$t[$1] <-> [$2] $3ms ($4s)%O")},

{"Channel List", pevt_chanlist_help, 3, 
N_("%C0%B%171List%187%B$t$(-16)1   $(-7)2 $3%O")},

{"Who Reply", pevt_gen_help, 1, 
N_("%C0%B%171Who%187%B$t$1%O")},

{"Garbage", pevt_gen_help, 1, 
N_("%C0%B%171Garbage%187%B$t$1%O")},

{"Silence", pevt_silence_help, 1, 
N_("%C0%B%171Silence%187%B$t$1%O")},

{"Netsplit", pevt_gen_help, 2, 
N_("%C0%B%171Netsplit%187%B$tNetsplit detected between servers $1 and $2.%O")},

{"Netsplit Users Gone", pevt_gen_help, 1, 
N_("%C0%B%171Netsplit%187%B$tUsers gone due to netsplit: $1%O")},

{"Stacked Join", pevt_gen_help, 2, 
N_("0%B-->%B$t$1Stacked join, new users on $1 is: $2%O")},
};
