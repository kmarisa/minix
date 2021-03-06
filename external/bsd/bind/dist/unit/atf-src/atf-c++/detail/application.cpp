//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if defined(HAVE_CONFIG_H)
#include "bconfig.h"
#endif

extern "C" {
#include <unistd.h>
}

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" {
#include "atf-c/defs.h"
}

#include "application.hpp"
#include "sanity.hpp"
#include "ui.hpp"

#if !defined(HAVE_VSNPRINTF_IN_STD)
namespace std {
using ::vsnprintf;
}
#endif // !defined(HAVE_VSNPRINTF_IN_STD)

namespace impl = atf::application;
#define IMPL_NAME "atf::application"

// ------------------------------------------------------------------------
// The "usage_error" class.
// ------------------------------------------------------------------------

impl::usage_error::usage_error(const char *fmt, ...)
    throw() :
    std::runtime_error("usage_error; message unformatted")
{
    va_list ap;

    va_start(ap, fmt);
    std::vsnprintf(m_text, sizeof(m_text), fmt, ap);
    va_end(ap);
}

impl::usage_error::~usage_error(void)
    throw()
{
}

const char*
impl::usage_error::what(void)
    const throw()
{
    return m_text;
}

// ------------------------------------------------------------------------
// The "application" class.
// ------------------------------------------------------------------------

impl::option::option(char ch,
                     const std::string& a,
                     const std::string& desc) :
    m_character(ch),
    m_argument(a),
    m_description(desc)
{
}

bool
impl::option::operator<(const impl::option& o)
    const
{
    return m_character < o.m_character;
}

impl::app::app(const std::string& description,
               const std::string& manpage,
               const std::string& global_manpage,
               const bool use_ui) :
    m_hflag(false),
    m_argc(-1),
    m_argv(NULL),
    m_prog_name(NULL),
    m_description(description),
    m_manpage(manpage),
    m_global_manpage(global_manpage),
    m_use_ui(use_ui)
{
}

impl::app::~app(void)
{
}

bool
impl::app::inited(void)
{
    return m_argc != -1;
}

impl::app::options_set
impl::app::options(void)
{
    options_set opts = specific_options();
    if (m_use_ui) {
        opts.insert(option('h', "", "Shows this help message"));
    }
    return opts;
}

std::string
impl::app::specific_args(void)
    const
{
    return "";
}

impl::app::options_set
impl::app::specific_options(void)
    const
{
    return options_set();
}

void
impl::app::process_option(int ch ATF_DEFS_ATTRIBUTE_UNUSED,
                          const char* arg ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::app::process_options(void)
{
    PRE(inited());

    std::string optstr;
#if defined(HAVE_GNU_GETOPT)
    optstr += '+'; // Turn on POSIX behavior.
#endif
    optstr += ':';
    {
        options_set opts = options();
        for (options_set::const_iterator iter = opts.begin();
             iter != opts.end(); iter++) {
            const option& opt = (*iter);

            optstr += opt.m_character;
            if (!opt.m_argument.empty())
                optstr += ':';
        }
    }

    int ch;
    const int old_opterr = ::opterr;
    ::opterr = 0;
    while ((ch = ::getopt(m_argc, m_argv, optstr.c_str())) != -1) {
        switch (ch) {
            case 'h':
                INV(m_use_ui);
                m_hflag = true;
                break;

            case ':':
                throw usage_error("Option -%c requires an argument.",
                                  ::optopt);

            case '?':
                throw usage_error("Unknown option -%c.", ::optopt);

            default:
                process_option(ch, ::optarg);
        }
    }
    m_argc -= ::optind;
    m_argv += ::optind;

    // Clear getopt state just in case the test wants to use it.
    opterr = old_opterr;
    optind = 1;
#if defined(HAVE_OPTRESET)
    optreset = 1;
#endif
}

void
impl::app::usage(std::ostream& os)
{
    PRE(inited());

    std::string args = specific_args();
    if (!args.empty())
        args = " " + args;
    os << ui::format_text_with_tag(std::string(m_prog_name) + " [options]" +
                                   args, "Usage: ", false) << "\n\n"
       << ui::format_text(m_description) << "\n\n";

    options_set opts = options();
    INV(!opts.empty());
    os << "Available options:\n";
    size_t coldesc = 0;
    for (options_set::const_iterator iter = opts.begin();
         iter != opts.end(); iter++) {
        const option& opt = (*iter);

        if (opt.m_argument.length() + 1 > coldesc)
            coldesc = opt.m_argument.length() + 1;
    }
    for (options_set::const_iterator iter = opts.begin();
         iter != opts.end(); iter++) {
        const option& opt = (*iter);

        std::string tag = std::string("    -") + opt.m_character;
        if (opt.m_argument.empty())
            tag += "    ";
        else
            tag += " " + opt.m_argument + "    ";
        os << ui::format_text_with_tag(opt.m_description, tag, false,
                                       coldesc + 10) << "\n";
    }
    os << "\n";

    std::string gmp;
    if (!m_global_manpage.empty())
        gmp = " and " + m_global_manpage;
    os << ui::format_text("For more details please see " + m_manpage +
                          gmp + ".")
       << "\n";
}

int
impl::app::run(int argc, char* const* argv)
{
    PRE(argc > 0);
    PRE(argv != NULL);

    m_argc = argc;
    m_argv = argv;

    m_argv0 = m_argv[0];

    m_prog_name = std::strrchr(m_argv[0], '/');
    if (m_prog_name == NULL)
        m_prog_name = m_argv[0];
    else
        m_prog_name++;

    // Libtool workaround: if running from within the source tree (binaries
    // that are not installed yet), skip the "lt-" prefix added to files in
    // the ".libs" directory to show the real (not temporary) name.
    if (std::strncmp(m_prog_name, "lt-", 3) == 0)
        m_prog_name += 3;

    const std::string bug =
        std::string("This is probably a bug in ") + m_prog_name +
        " or one of the libraries it uses.  Please report this problem to "
        PACKAGE_BUGREPORT " and provide as many details as possible "
        "describing how you got to this condition.";

    int errcode;
    try {
        int oldargc = m_argc;

        process_options();

        if (m_hflag) {
            INV(m_use_ui);
            if (oldargc != 2)
                throw usage_error("-h must be given alone.");

            usage(std::cout);
            errcode = EXIT_SUCCESS;
        } else
            errcode = main();
    } catch (const usage_error& e) {
        if (m_use_ui) {
            std::cerr << ui::format_error(m_prog_name, e.what()) << "\n"
                      << ui::format_info(m_prog_name, std::string("Type `") +
                                         m_prog_name + " -h' for more details.")
                      << "\n";
        } else {
            std::cerr << m_prog_name << ": ERROR: " << e.what() << "\n";
            std::cerr << m_prog_name << ": See " << m_manpage << " for usage "
                "details.\n";
        }
        errcode = EXIT_FAILURE;
    } catch (const std::runtime_error& e) {
        if (m_use_ui) {
            std::cerr << ui::format_error(m_prog_name, std::string(e.what()))
                      << "\n";
        } else {
            std::cerr << m_prog_name << ": ERROR: " << e.what() << "\n";
        }
        errcode = EXIT_FAILURE;
    } catch (const std::exception& e) {
        if (m_use_ui) {
            std::cerr << ui::format_error(m_prog_name, std::string("Caught "
                "unexpected error: ") + e.what() + "\n" + bug) << "\n";
        } else {
            std::cerr << m_prog_name << ": ERROR: Caught unexpected error: "
                      << e.what() << "\n";
        }
        errcode = EXIT_FAILURE;
    } catch (...) {
        if (m_use_ui) {
            std::cerr << ui::format_error(m_prog_name, std::string("Caught "
                "unknown error\n") + bug) << "\n";
        } else {
            std::cerr << m_prog_name << ": ERROR: Caught unknown error\n";
        }
        errcode = EXIT_FAILURE;
    }
    return errcode;
}
