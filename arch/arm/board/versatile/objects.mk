#/**
# Copyright (c) 2012 Jean-Christophe Dubois.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Jean-Christophe Dubois (jcd@tribudubois.net)
# @brief list of Versatile board objects.
# */

board-objs-y+=brd_defterm.o
board-objs-y+=brd_main.o
board-objs-$(CONFIG_VERSATILEPB_ONE_GUEST_VPB_DTS)+=dts/pb/one_guest_versatile.o
board-objs-$(CONFIG_VERSATILEPB_TWO_GUEST_VPB_DTS)+=dts/pb/two_guest_versatile.o

board-dtbs-y+=dts/pb/one_guest_versatile.dtb
board-dtbs-y+=dts/pb/two_guest_versatile.dtb
