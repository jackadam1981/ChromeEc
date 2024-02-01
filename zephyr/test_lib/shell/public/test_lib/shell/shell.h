#pragma once

#include "pw_assert/check.h"
#include "pw_string/string_builder.h"

#include <zephyr/shell/shell_dummy.h>

#include <iostream>

namespace cros::shell
{
class Shell {
    public:
	Shell(pw::StringBuilder &buffer, const struct shell *sh = nullptr);

	template <typename T> Shell &operator<<(const T &data)
	{
		if (buffer_.empty() || last_status_.ok()) {
			buffer_ << data;
			last_status_ = buffer_.status();
		}
		return *this;
	}

	typedef std::basic_ostream<char, std::char_traits<char> > OutType;
	typedef OutType &(*StandardEndLine)(OutType &);
	Shell &operator<<(StandardEndLine endl);

	pw::Status status() const;

    private:
	pw::StringBuilder &buffer_;
	const struct shell *sh_;
	pw::Status last_status_;
};
} /* namespace cros::shell */
