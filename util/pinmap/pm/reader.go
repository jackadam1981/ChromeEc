// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package pm

import (
	"fmt"
)

type Reader interface {
	Name() string
	Read(arg string, chip string) (*Pins, error)
}

// Registered list of readers.
var readerList []Reader

func ReadPins(reader, chip, arg string) (*Pins, error) {
	for _, r := range readerList {
		if r.Name() == reader {
			return r.Read(chip, arg)
		}
	}
	return nil, fmt.Errorf("%s: unknown reader", reader)
}

func Readers() []string {
	var l []string
	for _, r := range readerList {
		l = append(l, r.Name())
	}
	return l
}

func RegisterReader(reader Reader) {
	readerList = append(readerList, reader)
}
