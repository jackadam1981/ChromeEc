/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "pw_assert/check.h"
#include "pw_string/string_builder.h"

#include <zephyr/shell/shell_dummy.h>

#include <iostream>
#include <string>

namespace cros::shell
{

/**
 * @brief Wrapper class for the shell struct.
 *
 * Usage:
 * @code
 * pw::StringBuffer<128> cmd_buffer;
 * cros::shell::Shell shell(cmd_buffer);
 *
 * shell << "accelinfo " << 0 << std::endl;
 * @endcode
 */
class Shell {
    public:
	/**
	 * @param buffer The buffer to use for writing to the shell
	 * @param sh The shell to use, may be null if the dummy shell is enabled
	 */
	explicit Shell(pw::StringBuilder &buffer,
		       const struct shell *sh = nullptr);

	/**
	 * Write data to the shell. Use std::endl to send the command.
	 *
	 * @tparam T Data type
	 * @param data Data to write to the buffer
	 * @return This shell instance
	 */
	template <typename T> Shell &operator<<(const T &data)
	{
		if (buffer_.empty() || last_status_.ok()) {
			buffer_ << data;
			last_status_ = buffer_.status();
		}
		return *this;
	}

	/** Type used as the output type of the shell */
	using OutType = std::basic_ostream<char, std::char_traits<char> >;

	/** Function signature of std::endl */
	using StandardEndLine = OutType &(*)(OutType &);

	/**
	 * Specialization function to handle flushing to the shell via std::endl
	 */
	Shell &operator<<(StandardEndLine endl);

	/**
	 * @return The current status of the shell.
	 */
	pw::Status status() const;

    private:
	pw::StringBuilder &buffer_;
	const struct shell *sh_;
	pw::Status last_status_;
};

} /* namespace cros::shell */
