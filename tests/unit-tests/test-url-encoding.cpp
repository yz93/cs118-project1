/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California
 *
 * This file is part of Simple BT.
 * See AUTHORS.md for complete list of Simple BT authors and contributors.
 *
 * NSL is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NSL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NSL, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \author Yingdi Yu <yingdi@cs.ucla.edu>
 */

#include "http/url-encoding.hpp"

#include "boost-test.hpp"

namespace sbt {
namespace test {

BOOST_AUTO_TEST_SUITE(TestUrlEncoding)

BOOST_AUTO_TEST_CASE(Basic)
{
  uint8_t input1[] = {0x01, 0x2C, 0x2D, 0x2E, 0x2F};
  BOOST_CHECK_EQUAL(url::encode(input1, 5), "%01%2C-.%2F");

  uint8_t input2[] = {0x30, 0x39, 0x3A, 0x40, 0x41};
  BOOST_CHECK_EQUAL(url::encode(input2, 5), "09%3A%40A");

  uint8_t input3[] = {0x5A, 0x5B, 0x5E, 0x5F, 0x60, 0x61};
  BOOST_CHECK_EQUAL(url::encode(input3, 6), "Z%5B%5E_%60a");

  uint8_t input4[] = {0x7A, 0x7B, 0x7D, 0x7E, 0x7F};
  BOOST_CHECK_EQUAL(url::encode(input4, 5), "z%7B%7D~%7F");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sbt
