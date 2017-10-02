#pragma once

#include "common/common_pch.h"

#include "mkvtoolnix-gui/header_editor/value_page.h"

class QLineEdit;

namespace mtx { namespace gui { namespace HeaderEditor {

class Tab;

class UnsignedIntegerValuePage: public ValuePage {
public:
  QLineEdit *m_leValue{};
  uint64_t m_originalValue{};

public:
  UnsignedIntegerValuePage(Tab &parent, PageBase &topLevelPage, EbmlMaster &master, EbmlCallbacks const &callbacks, translatable_string_c const &title, translatable_string_c const &description);
  virtual ~UnsignedIntegerValuePage();

  virtual QWidget *createInputControl() override;
  virtual QString originalValueAsString() const override;
  virtual QString currentValueAsString() const override;
  virtual void resetValue() override;
  virtual bool validateValue() const override;
  virtual void copyValueToElement() override;
};

}}}
