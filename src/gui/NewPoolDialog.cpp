#include "NewPoolDialog.h"

#include "ui_newpooldialog.h"

namespace WalletGui {

NewPoolDialog::NewPoolDialog(QWidget* _parent) : QDialog(_parent), m_ui(new Ui::NewPoolDialog) {
  m_ui->setupUi(this);
}

NewPoolDialog::~NewPoolDialog() {
}

QString NewPoolDialog::getHost() const {
  return m_ui->m_hostEdit->text().trimmed();
}

quint16 NewPoolDialog::getPort() const {
  return m_ui->m_portSpin->value();
}

}
