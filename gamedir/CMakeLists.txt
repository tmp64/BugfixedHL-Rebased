set( TXT_UNICODE_CONVERT_REGEX "\\.utf8\\.txt$" )
set( TXT_UNICODE_CONVERT_REPLACE ".txt" )

set( CLIENT_FILES
	resource/GameMenu.res

	sound/misc/talk_bhl_fallback.wav

	sprites/ag_ctf.spr
	sprites/ag_ctf_score.spr

	ui/gfx/circle_bg.tga
	ui/gfx/circle_fg.tga
	ui/gfx/cross_bg.tga
	ui/gfx/default_avatar.tga
	ui/gfx/hud/speaker.tga
	ui/gfx/hud/speaker_self.tga
	ui/gfx/muted_icon32.tga
	ui/gfx/vgui/800corner1.tga
	ui/gfx/vgui/800corner2.tga
	ui/gfx/vgui/800corner3.tga
	ui/gfx/vgui/800corner4.tga

	ui/resource/Chat.res
	ui/resource/ChatScheme.res
	ui/resource/ChatSchemeBase.res
	ui/resource/ClientScheme.res
	ui/resource/ClientSchemeBase.res
	ui/resource/ClientSourceScheme.res
	ui/resource/ClientSourceSchemeBase.res
	ui/resource/MOTD.res
	ui/resource/ScorePanel.res
	ui/resource/SpectatorInfoPanel.res
	ui/resource/TeamMenu.res

	ui/resource/language/bugfixedhl_english.utf8.txt
	ui/resource/language/bugfixedhl_russian.utf8.txt
	ui/resource/language/chat_english.utf8.txt
	ui/resource/language/chat_french.utf8.txt
	ui/resource/language/chat_german.utf8.txt
	ui/resource/language/chat_italian.utf8.txt
	ui/resource/language/chat_korean.utf8.txt
	ui/resource/language/chat_russian.utf8.txt
	ui/resource/language/chat_schinese.utf8.txt
	ui/resource/language/chat_spanish.utf8.txt
	ui/resource/language/chat_tchinese.utf8.txt

	ui/resource/open_source_software.txt

	ui/resource/options/AboutSubOptions.res
	ui/resource/options/ChatSubOptions.res
	ui/resource/options/ColorPicker.res
	ui/resource/options/CrosshairSubOptions.res
	ui/resource/options/GeneralSubOptions.res
	ui/resource/options/HudSubOptions.res
	ui/resource/options/HudSubOptionsColors.res
	ui/resource/options/ModelSubOptions.res
	ui/resource/options/ScoreboardSubOptions.res

	ui/resource/updater/UpdateDownloadStatusDialog.res
	ui/resource/updater/UpdateFileProgressDialog.res
	ui/resource/updater/UpdateFileReplaceDialog.res
	ui/resource/updater/UpdateNotificationDialog.res

	ui/scripts/HudAnimations.txt
	ui/scripts/HudLayout.res
	ui/scripts/HudLayoutBase.res

	commandmenu_default.txt
)

set( SERVER_FILES
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cactanimating.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cagrunt.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cairtank.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cambientgeneric.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-capache.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-capachehvr.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cautotrigger.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbabycrab.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbarnacle.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbarney.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseanimating.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasebutton.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasedelay.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasedmstart.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasedoor.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseentity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasemonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseplattrain.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseplayer.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseplayerammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseplayeritem.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseplayerweapon.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasespectator.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasetoggle.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbasetrigger.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbaseturret.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbeam.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbigmomma.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbloater.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cblood.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbloodsplat.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbmortar.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbreakable.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbubbling.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbullsquid.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cbuttontarget.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cchangelevel.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccine2heavyweapons.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccine2scientist.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccine2slave.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccine3barney.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccine3scientist.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccineai.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccinebarney.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccineblood.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccinemonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccinepanther.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccinescientist.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cclientfog.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccontroller.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccontrollerheadball.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccontrollerzapball.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccorpse.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccrossbow.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccrossbowammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccrossbowbolt.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccrowbar.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccycler.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccyclerprobe.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ccyclersprite.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cdeadbarney.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cdeadhev.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cdeadhgrunt.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cdeadscientist.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cdecal.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cegon.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cegonammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvbeverage.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvexplosion.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvfunnel.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvglobal.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvlight.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvshooter.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvsound.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cenvspark.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfade.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfireanddie.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cflockingflyer.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cflockingflyerflock.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cflyingmonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfrictionmodifier.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncconveyor.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncillusionary.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncmonsterclip.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncmortarfield.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncplat.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncplatrot.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncrotating.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctank.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctankcontrols.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctankgun.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctanklaser.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctankmortar.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctankrocket.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctrackauto.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctrackchange.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctracktrain.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctrain.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfunctraincontrols.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncvehicle.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncvehiclecontrols.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncwall.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfuncwalltoggle.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cfurniture.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgamecounter.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgamecounterset.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgameend.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgameplayerequip.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgameplayerhurt.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgameplayerteam.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgameplayerzone.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgamerules.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgamescore.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgameteammaster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgameteamset.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgametext.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgargantua.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgauss.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgaussammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgenericcycler.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgenericmonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgib.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgibshooter.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cglock.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cglockammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cglow.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgman.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cgrenade.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cguntarget.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chalflifemultiplay.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chalfliferules.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chalflifeteamplay.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chandgrenade.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chassassin.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cheadcrab.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chealthkit.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chgrunt.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chgruntrepel.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chgun.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-chornet.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-choundeye.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cichthyosaur.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cinfobm.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cinfointermission.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cislave.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-citem.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-citemantidote.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-citembattery.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-citemlongjump.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-citemsecurity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-citemsoda.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-citemsuit.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cladder.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-claser.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-claserspot.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cleech.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-clegacycinemonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-clight.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-clightning.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmessage.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cminiturret.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmomentarydoor.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmomentaryrotbutton.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmonstermaker.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmortar.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmp5.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmp5ammoclip.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmp5ammogrenade.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmp5chainammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmultimanager.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmultiplaybusters.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cmultisource.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cnihilanth.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cnihilanthhvr.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cnodeent.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cnodeviewer.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cnullentity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cosprey.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cpathcorner.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cpathtrack.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cpendulum.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cplattrigger.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cpointentity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cpushable.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cpython.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cpythonammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crat.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crecharge.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crenderfxmanager.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crevertsaved.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-croach.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crotbutton.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crotdoor.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crpg.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crpgammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crpgrocket.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crulebrushentity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cruleentity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-crulepointentity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csatchel.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csatchelcharge.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cscientist.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cscriptedsentence.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csentry.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cshake.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cshotgun.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cshotgunammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cshower.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csittingscientist.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csmoker.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csoundent.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cspeaker.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cspiral.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cspraycan.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csprite.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csquadmonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csqueak.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csqueakgrenade.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-csquidspit.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cstomp.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cstripweapons.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctalkmonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctargetcdaudio.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctentacle.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctentaclemaw.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctesteffect.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctesthull.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggercamera.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggercdaudio.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggerchangetarget.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggercounter.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggerendsection.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggergravity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggerhurt.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggermonsterjump.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggermultiple.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggeronce.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggerpush.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggerrelay.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggersave.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggerteleport.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctriggervolume.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctripbeam.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctripmine.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-ctripminegrenade.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cturret.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cwallhealth.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cweaponbox.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cweaponcycler.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cworld.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cworlditem.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cwreckage.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxenhair.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxenhull.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxenplight.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxenspore.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxensporelarge.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxensporemed.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxensporesmall.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxentree.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-cxentreetrigger.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-czombie.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseanimating.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseentity.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbasemonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseplattrain.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseplayer.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseplayerammo.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseplayeritem.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseplayerweapon.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbasetoggle.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cbaseturret.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-ccinemonster.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cfuncplat.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cfunctank.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cfunctrackchange.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cgamerules.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cgibshooter.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cgrenade.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-chalflifemultiplay.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cheadcrab.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-citem.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-cnihilanthhvr.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-common.txt
	addons/amxmodx/data/gamedata/common.games/custom/bhl-offsets-virtual-ctalkmonster.txt
	addons/amxmodx/scripting/include/bugfixedapi.inc
	
	delta.lst
)

function(bhl_install_files component_name file_list)
	set( converted_txt_files )

	foreach( rel_file_path IN LISTS file_list)
		cmake_path( HAS_PARENT_PATH rel_file_path has_parent_path )

		if( has_parent_path)
			cmake_path( GET rel_file_path PARENT_PATH parent_path )
		else()
			set( parent_path "." )
		endif()

		if( rel_file_path MATCHES ${TXT_UNICODE_CONVERT_REGEX})
			# Convert file into UTF-16 LE for VGUI2
			string( REGEX REPLACE ${TXT_UNICODE_CONVERT_REGEX} ${TXT_UNICODE_CONVERT_REPLACE} converted_rel_file_path ${rel_file_path} )

			file( MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${parent_path}" )
			set( file_to_copy "${CMAKE_CURRENT_BINARY_DIR}/${converted_rel_file_path}" )

			add_custom_command(
				OUTPUT ${file_to_copy}
				COMMAND
					Python::Interpreter
					"${CMAKE_SOURCE_DIR}/scripts/reencode_text_file.py"
					"${CMAKE_CURRENT_SOURCE_DIR}/${rel_file_path}"
					${file_to_copy}
				DEPENDS
					"${CMAKE_SOURCE_DIR}/scripts/reencode_text_file.py"
					"${CMAKE_CURRENT_SOURCE_DIR}/${rel_file_path}"
			)

			list( APPEND converted_txt_files ${file_to_copy} )
		else()
			# Copy the file as-is
			set( file_to_copy "${CMAKE_CURRENT_SOURCE_DIR}/${rel_file_path}" )
		endif()

		install(
			FILES ${file_to_copy}
			DESTINATION ${parent_path}
			COMPONENT ${component_name}
		)
	endforeach()

	if( converted_txt_files )
		# Create custom target to convert the files
		add_custom_target( convert_utf_files_${component_name} DEPENDS ${converted_txt_files} )
	endif()
endfunction()


function(bhl_install_target component_name target_name install_dir)
	install(
		FILES $<TARGET_FILE:${target_name}>
		DESTINATION ${install_dir}
		COMPONENT ${component_name}
	)

	if( CMAKE_CXX_LINKER_SUPPORTS_PDB )
		install(
			FILES $<TARGET_PDB_FILE:${target_name}>
			DESTINATION ${install_dir}
			COMPONENT ${component_name}
			OPTIONAL
		)
	endif()
endfunction()

# Client
bhl_install_files( client "${CLIENT_FILES}" )
bhl_install_target( client client "cl_dlls" )
add_dependencies( client convert_utf_files_client )

# Server
bhl_install_files( server "${SERVER_FILES}" )
bhl_install_target( server server "dlls" )
bhl_install_target( server bugfixedapi_amxx "addons/amxmodx/modules" )
