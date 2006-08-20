SUBDIRS = sting

AM_CFLAGS = -Wall

bin_PROGRAMS = heretic

header_files = am_data.h am_map.h ct_chat.h doomdata.h doomdef.h drcoord.h \
	dstrings.h i_header.h info.h i_audio.h p_local.h p_spec.h r_local.h \
	sounds.h soundst.h m_fixed.h i_music.h i_video.h i_sound.h i_net.h \
	i_system.h i_qmus2mid.h m_swap.h

heretic_SOURCES =  am_map.c ct_chat.c d_main.c d_net.c f_finale.c g_game.c \
	info.c in_lude.c m_misc.c mn_menu.c p_ceilng.c p_doors.c p_enemy.c \
	p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c \
	p_pspr.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c \
	p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_things.c \
	sb_bar.c sounds.c tables.c v_video.c w_wad.c z_zone.c soundst.c \
	i_system.c i_audio.c m_fixed.c i_music.c i_video.c i_sound.c i_net.c \
	i_net_unix.c i_net_sting.c i_qmus2mid.c m_swap.c

EXTRA_DIST = $(header_files)
