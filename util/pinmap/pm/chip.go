// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package pm

type Chip interface {
	Name() string
	EnabledNodes() []string
	Adc(pin string) string
	Gpio(pin string) string
	I2c(pin string) string
	Pwm(pin string) string
}

// Registered list of chips
var chipList []Chip

func RegisterChip(chip Chip) {
	chipList = append(chipList, chip)
}

func NewChip(name string) Chip {
	for _, c := range chipList {
		if c.Name() == name {
			return c
		}
	}
	return nil
}

func Chips() []string {
	var l []string
	for _, c := range chipList {
		l = append(l, c.Name())
	}
	return l
}
