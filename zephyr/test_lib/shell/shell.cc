/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test_lib/shell/shell.hh"

namespace cros::shell
{

Shell::Shell(pw::StringBuilder &buffer, const struct shell *sh)
	: buffer_(buffer)
{
	if (sh == nullptr) {
#ifdef CONFIG_SHELL_BACKEND_DUMMY
		sh_ = shell_backend_dummy_get_ptr();
#else
		PW_CRASH("Shell cannot be null");
#endif
	}
	sh_ = sh;
	last_status_ = pw::OkStatus();
}

pw::Status Shell::status() const
{
	return last_status_;
}

Shell &Shell::operator<<(Shell::StandardEndLine endl)
{
	if (last_status_.ok()) {
		if (shell_execute_cmd(sh_, buffer_.c_str()) != 0) {
			last_status_ = pw::Status::Internal();
		}
	}
	buffer_.clear();
	return *this;
}

} /* namespace cros::shell */
