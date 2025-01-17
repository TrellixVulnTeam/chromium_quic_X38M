// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_FACTORY_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

class AccountId;
class Profile;

namespace chromeos {

class BulkPrintersCalculator;

// Dispenses BulkPrintersCalculator objects based on account id or for device
// context.  Access to this object should be sequenced.
class BulkPrintersCalculatorFactory {
 public:
  // It never returns nullptr.
  static BulkPrintersCalculatorFactory* Get();

  BulkPrintersCalculatorFactory();

  // Returns a WeakPtr to the BulkPrintersCalculator registered for
  // |account_id|.
  // If requested BulkPrintersCalculator does not exist, the output depends on
  // the given parameter |create_if_not_exists|. If it is true, the object is
  // created and registered, otherwise nullptr is returned.
  // The returned object remains valid until RemoveForUserId or Shutdown is
  // called.
  base::WeakPtr<BulkPrintersCalculator> GetForAccountId(
      const AccountId& account_id,
      bool create_if_not_exists);

  // Returns a WeakPtr to the BulkPrintersCalculator registered for |profile|
  // which could be nullptr if |profile| does not map to a valid AccountId.
  // If requested BulkPrintersCalculator does not exist, the output depends on
  // the given parameter |create_if_not_exists|. If it is true, the object is
  // created and registered, otherwise nullptr is returned.
  // The returned object remains valid until RemoveForUserId or Shutdown is
  // called.
  base::WeakPtr<BulkPrintersCalculator> GetForProfile(
      Profile* profile,
      bool create_if_not_exists);

  // Returns a WeakPtr to the BulkPrintersCalculator registered for the device.
  // If requested BulkPrintersCalculator does not exist, the output depends on
  // the given parameter |create_if_not_exists|. If it is true, the object is
  // created and registered, otherwise nullptr is returned.
  // The returned object remains valid until Shutdown is called.
  base::WeakPtr<BulkPrintersCalculator> GetForDevice(bool create_if_not_exists);

  // Deletes the BulkPrintersCalculator registered for |account_id|.
  void RemoveForUserId(const AccountId& account_id);

  // Tear down all BulkPrintersCalculator objects.
  void Shutdown();

 private:
  ~BulkPrintersCalculatorFactory();

  std::map<AccountId, std::unique_ptr<BulkPrintersCalculator>>
      printers_by_user_;
  std::unique_ptr<BulkPrintersCalculator> device_printers_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BulkPrintersCalculatorFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_FACTORY_H_
