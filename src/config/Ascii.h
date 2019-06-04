// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2019, The Qwertycoin Developers
//
// Please see the included LICENSE file for more information

#pragma once

const std::string windowsAsciiArt =
    "\n                                                              \n"
    "                         _                   _                  \n"
    "                        | |                 (_)                 \n"
    "  __ ___      _____ _ __| |_ _   _  ___ ___  _ _ __             \n"
    " / _` \\ \\ /\\ / / _ \\ '__| __| | | |/ __/ _ \\| | '_\\       \n"
    "| (_| |\\ V  V /  __/ |  | |_| |_| | (_| (_) | | | | |          \n"
    " \\__, | \\_/\\_/ \\___|_|   \\__|\\__, |\\___\\___/|_|_| |_|   \n"
    "    | |                       __/ |                             \n"
    "    |_|                      |___/                              \n"
    "                                                                \n";

const std::string nonWindowsAsciiArt =
    "\n                                                                                 \n"
    " ██████╗ ██╗    ██╗███████╗██████╗ ████████╗██╗   ██╗ ██████╗ ██████╗ ██╗███╗   ██╗\n"
    "██╔═══██╗██║    ██║██╔════╝██╔══██╗╚══██╔══╝╚██╗ ██╔╝██╔════╝██╔═══██╗██║████╗  ██║\n"
    "██║   ██║██║ █╗ ██║█████╗  ██████╔╝   ██║    ╚████╔╝ ██║     ██║   ██║██║██╔██╗ ██║\n"
    "██║▄▄ ██║██║███╗██║██╔══╝  ██╔══██╗   ██║     ╚██╔╝  ██║     ██║   ██║██║██║╚██╗██║\n"
    "╚██████╔╝╚███╔███╔╝███████╗██║  ██║   ██║      ██║   ╚██████╗╚██████╔╝██║██║ ╚████║\n"
    " ╚══▀▀═╝  ╚══╝╚══╝ ╚══════╝╚═╝  ╚═╝   ╚═╝      ╚═╝    ╚═════╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝\n"
"                                                                                   \n";

/*!
    Windows has some characters it won't display in a terminal. If your ascii
    art works fine on Windows and Linux terminals, just replace 'asciiArt' with
    the art itself, and remove these two #ifdefs and above ascii arts.
*/
#ifdef _WIN32
const std::string asciiArt = windowsAsciiArt;
#else
const std::string asciiArt = nonWindowsAsciiArt;
#endif
