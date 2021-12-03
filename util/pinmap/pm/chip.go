// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package pm

// Chip represents an Embedded Controller IC, where
// pin references can be used to lookup various types of
// pin usages such as I2C buses, GPIOs etc.
type Chip interface {
	Name() string
	EnabledNodes() []string
	Adc(pin string) string
	Gpio(pin string) string
	I2c(pin string) string
	Pwm(pin string) string
}

// chipList contains a list of registered chips.
// Each chip has a unique name that is used to match it.
var chipList []Chip

// RegisterChip adds this chip into the list of registered chips.
func RegisterChip(chip Chip) {
	chipList = append(chipList, chip)
}

// FindChip returns the registered chip matching this name, or nil
// if none are found.
func FindChip(name string) Chip {
	for _, c := range chipList {
		if c.Name() == name {
			return c
		}
	}
	return nil
}

// Chips returns the list of names of the registered chips.
func Chips() []string {
	var l []string
	for _, c := range chipList {
		l = append(l, c.Name())
	}
	return l
}
