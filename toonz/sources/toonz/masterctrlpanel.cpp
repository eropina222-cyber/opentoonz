#include "masterctrlpanel.h"

// Qt includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

// Toonz includes
#include "tapp.h"
#include "toonz/tstageobject.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"

//=============================================================================
// JoystickWidget Implementation
//=============================================================================

JoystickWidget::JoystickWidget(QWidget *parent)
    : QWidget(parent)
    , m_normalizedX(0.0)
    , m_normalizedY(0.0)
    , m_isDragging(false) {
  setMinimumSize(200, 200);
  setStyleSheet("background-color: #2a2a2a; border: 1px solid #555;");
}

void JoystickWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  int size = width();
  int center = size / 2;
  int radius = size / 2 - 10;

  // Draw outer circle
  painter.setPen(QPen(Qt::gray, 2));
  painter.drawEllipse(center - radius, center - radius, radius * 2,
                      radius * 2);

  // Draw crosshair
  painter.setPen(QPen(Qt::darkGray, 1));
  painter.drawLine(center - radius / 2, center, center + radius / 2, center);
  painter.drawLine(center, center - radius / 2, center, center + radius / 2);

  // Draw center point
  painter.setPen(Qt::white);
  painter.drawEllipse(center - 3, center - 3, 6, 6);

  // Draw joystick handle
  int handleX = center + (int)(m_normalizedX * radius * 0.8);
  int handleY = center - (int)(m_normalizedY * radius * 0.8); // Invert Y
  painter.setPen(Qt::NoPen);
  painter.setBrush(QBrush(Qt::cyan));
  painter.drawEllipse(handleX - 8, handleY - 8, 16, 16);

  // Draw position text
  painter.setPen(Qt::white);
  QFont font = painter.font();
  font.setPointSize(8);
  painter.setFont(font);
  QString posText = QString("X: %1\nY: %2")
                        .arg(m_normalizedX, 0, 'f', 2)
                        .arg(m_normalizedY, 0, 'f', 2);
  painter.drawText(10, size - 30, posText);
}

void JoystickWidget::mousePressEvent(QMouseEvent *event) {
  m_isDragging = true;
  updatePosition(event);
}

void JoystickWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_isDragging) {
    updatePosition(event);
  }
}

void JoystickWidget::mouseReleaseEvent(QMouseEvent *event) {
  m_isDragging = false;
  // Center joystick when released
  m_normalizedX = 0.0;
  m_normalizedY = 0.0;
  update();
  emit joystickMoved(m_normalizedX, m_normalizedY);
}

void JoystickWidget::updatePosition(QMouseEvent *event) {
  int size = width();
  int center = size / 2;
  int radius = size / 2 - 10;

  int dx = event->pos().x() - center;
  int dy = center - event->pos().y(); // Invert Y

  double distance = std::sqrt(dx * dx + dy * dy);
  double maxDistance = radius * 0.8;

  if (distance > maxDistance) {
    double angle = std::atan2(dy, dx);
    dx = (int)(std::cos(angle) * maxDistance);
    dy = (int)(std::sin(angle) * maxDistance);
  }

  m_normalizedX = (double)dx / (radius * 0.8);
  m_normalizedY = (double)dy / (radius * 0.8);

  // Clamp to [-1, 1]
  m_normalizedX = std::max(-1.0, std::min(1.0, m_normalizedX));
  m_normalizedY = std::max(-1.0, std::min(1.0, m_normalizedY));

  update();
  emit joystickMoved(m_normalizedX, m_normalizedY);
}

void JoystickWidget::setNormalized(double x, double y) {
  m_normalizedX = std::max(-1.0, std::min(1.0, x));
  m_normalizedY = std::max(-1.0, std::min(1.0, y));
  update();
}

//=============================================================================
// MasterControllerPanel Implementation
//=============================================================================

MasterControllerPanel::MasterControllerPanel(QWidget *parent)
    : QDockWidget("Master Controller", parent) {
  memset(m_posesSaved, 0, sizeof(m_posesSaved));
  setupUI();
  connectSignals();
  setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}

MasterControllerPanel::~MasterControllerPanel() {}

void MasterControllerPanel::setupUI() {
  m_mainWidget = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(m_mainWidget);
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setSpacing(10);

  // Joystick
  m_joystick = new JoystickWidget(this);
  mainLayout->addWidget(m_joystick);

  // Position, Rotation, Scale labels
  QGroupBox *infoGroup = new QGroupBox("Current Values", this);
  QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);

  m_positionLabel = new QLabel("Position: X=0.00, Y=0.00", this);
  m_rotationLabel = new QLabel("Rotation: 0.00°", this);
  m_scaleLabel = new QLabel("Scale: X=1.00, Y=1.00", this);

  m_positionLabel->setStyleSheet("color: #00FF00;");
  m_rotationLabel->setStyleSheet("color: #00FF00;");
  m_scaleLabel->setStyleSheet("color: #00FF00;");

  infoLayout->addWidget(m_positionLabel);
  infoLayout->addWidget(m_rotationLabel);
  infoLayout->addWidget(m_scaleLabel);

  mainLayout->addWidget(infoGroup);

  // Pose buttons
  QGroupBox *poseGroup = new QGroupBox("Save Poses (4 Corners)", this);
  QGridLayout *poseLayout = new QGridLayout(poseGroup);

  // Top-left button
  m_savePoseButtons[0] = new QPushButton("↖ TL", this);
  m_savePoseButtons[0]->setToolTip("Top-Left Pose");
  poseLayout->addWidget(m_savePoseButtons[0], 0, 0);

  // Top-right button
  m_savePoseButtons[1] = new QPushButton("↗ TR", this);
  m_savePoseButtons[1]->setToolTip("Top-Right Pose");
  poseLayout->addWidget(m_savePoseButtons[1], 0, 1);

  // Bottom-left button
  m_savePoseButtons[2] = new QPushButton("↙ BL", this);
  m_savePoseButtons[2]->setToolTip("Bottom-Left Pose");
  poseLayout->addWidget(m_savePoseButtons[2], 1, 0);

  // Bottom-right button
  m_savePoseButtons[3] = new QPushButton("↘ BR", this);
  m_savePoseButtons[3]->setToolTip("Bottom-Right Pose");
  poseLayout->addWidget(m_savePoseButtons[3], 1, 1);

  mainLayout->addWidget(poseGroup);

  // Reset button
  m_resetButton = new QPushButton("Reset All Poses", this);
  mainLayout->addWidget(m_resetButton);

  mainLayout->addStretch();

  setWidget(m_mainWidget);
}

void MasterControllerPanel::connectSignals() {
  connect(m_joystick, SIGNAL(joystickMoved(double, double)), this,
          SLOT(onJoystickMoved(double, double)));

  connect(m_savePoseButtons[0], SIGNAL(clicked()), this,
          SLOT(onSavePoseTopLeft()));
  connect(m_savePoseButtons[1], SIGNAL(clicked()), this,
          SLOT(onSavePoseTopRight()));
  connect(m_savePoseButtons[2], SIGNAL(clicked()), this,
          SLOT(onSavePoseBottomLeft()));
  connect(m_savePoseButtons[3], SIGNAL(clicked()), this,
          SLOT(onSavePoseBottomRight()));

  connect(m_resetButton, SIGNAL(clicked()), this, SLOT(onResetPoses()));
}

void MasterControllerPanel::onJoystickMoved(double normalizedX,
                                            double normalizedY) {
  // Normalize from [-1, 1] to [0, 1] for bilinear interpolation
  double u = (normalizedX + 1.0) / 2.0; // 0 to 1
  double v = (normalizedY + 1.0) / 2.0; // 0 to 1

  updateFromPoses(u, v);
}

void MasterControllerPanel::updateFromPoses(double u, double v) {
  // Check if at least one pose is saved
  bool anyPoseSaved = false;
  for (int i = 0; i < 4; i++) {
    if (m_posesSaved[i]) {
      anyPoseSaved = true;
      break;
    }
  }

  if (!anyPoseSaved) {
    return; // No poses saved yet
  }

  Pose interpolated = interpolateBilinear(u, v);
  updateLabels(interpolated);
  applyPoseToObject(interpolated);
}

MasterControllerPanel::Pose MasterControllerPanel::interpolateBilinear(
    double u, double v) const {
  Pose result;

  // Bilinear interpolation between 4 corner poses
  // u=0, v=0 -> BottomLeft
  // u=1, v=0 -> BottomRight
  // u=0, v=1 -> TopLeft
  // u=1, v=1 -> TopRight

  double w00 = (1 - u) * (1 - v); // BL weight
  double w10 = u * (1 - v);       // BR weight
  double w01 = (1 - u) * v;       // TL weight
  double w11 = u * v;             // TR weight

  // Use saved poses or defaults
  const Pose &bl = m_posesSaved[2] ? m_poses[2] : Pose();
  const Pose &br = m_posesSaved[3] ? m_poses[3] : Pose();
  const Pose &tl = m_posesSaved[0] ? m_poses[0] : Pose();
  const Pose &tr = m_posesSaved[1] ? m_poses[1] : Pose();

  result.x = w00 * bl.x + w10 * br.x + w01 * tl.x + w11 * tr.x;
  result.y = w00 * bl.y + w10 * br.y + w01 * tl.y + w11 * tr.y;
  result.rotation =
      w00 * bl.rotation + w10 * br.rotation + w01 * tl.rotation + w11 * tr.rotation;
  result.scaleX =
      w00 * bl.scaleX + w10 * br.scaleX + w01 * tl.scaleX + w11 * tr.scaleX;
  result.scaleY =
      w00 * bl.scaleY + w10 * br.scaleY + w01 * tl.scaleY + w11 * tr.scaleY;

  return result;
}

void MasterControllerPanel::applyPoseToObject(const Pose &pose) {
  TApp *app = TApp::instance();
  if (!app) return;

  TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
  if (!xsheetHandle) return;

  TColumnHandle *columnHandle = app->getCurrentColumn();
  if (!columnHandle) return;

  int col = columnHandle->getColumnIndex();
  if (col < 0) return;

  TStageObjectId objId(col);
  TStageObject *obj = xsheetHandle->getXsheet()->getStageObject(objId);
  if (!obj) return;

  TFrameHandle *frameHandle = app->getCurrentFrame();
  int frame = frameHandle->getFrameIndex();

  // Apply position
  obj->setX(pose.x, frame);
  obj->setY(pose.y, frame);

  // Apply rotation
  obj->setRotation(pose.rotation, frame);

  // Apply scale
  obj->setScaleX(pose.scaleX, frame);
  obj->setScaleY(pose.scaleY, frame);
}

void MasterControllerPanel::savePose(int poseIndex) {
  TApp *app = TApp::instance();
  if (!app) return;

  TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
  if (!xsheetHandle) return;

  TColumnHandle *columnHandle = app->getCurrentColumn();
  if (!columnHandle) return;

  int col = columnHandle->getColumnIndex();
  if (col < 0) return;

  TStageObjectId objId(col);
  TStageObject *obj = xsheetHandle->getXsheet()->getStageObject(objId);
  if (!obj) return;

  TFrameHandle *frameHandle = app->getCurrentFrame();
  int frame = frameHandle->getFrameIndex();

  // Save current object state
  m_poses[poseIndex].x = obj->getX(frame);
  m_poses[poseIndex].y = obj->getY(frame);
  m_poses[poseIndex].rotation = obj->getRotation(frame);
  m_poses[poseIndex].scaleX = obj->getScaleX(frame);
  m_poses[poseIndex].scaleY = obj->getScaleY(frame);

  m_posesSaved[poseIndex] = true;

  // Update button style to show it's saved
  m_savePoseButtons[poseIndex]->setStyleSheet(
      "QPushButton { background-color: #00AA00; color: white; }");
}

void MasterControllerPanel::onSavePoseTopLeft() {
  savePose(0);
}

void MasterControllerPanel::onSavePoseTopRight() {
  savePose(1);
}

void MasterControllerPanel::onSavePoseBottomLeft() {
  savePose(2);
}

void MasterControllerPanel::onSavePoseBottomRight() {
  savePose(3);
}

void MasterControllerPanel::onResetPoses() {
  memset(m_posesSaved, 0, sizeof(m_posesSaved));

  for (int i = 0; i < 4; i++) {
    m_poses[i] = Pose();
    m_savePoseButtons[i]->setStyleSheet("");
  }

  m_joystick->setNormalized(0.0, 0.0);
  updateLabels(Pose());
}

void MasterControllerPanel::updateLabels(const Pose &currentPose) {
  m_positionLabel->setText(
      QString("Position: X=%1, Y=%2")
          .arg(currentPose.x, 0, 'f', 2)
          .arg(currentPose.y, 0, 'f', 2));

  m_rotationLabel->setText(
      QString("Rotation: %1°").arg(currentPose.rotation, 0, 'f', 2));

  m_scaleLabel->setText(
      QString("Scale: X=%1, Y=%2")
          .arg(currentPose.scaleX, 0, 'f', 2)
          .arg(currentPose.scaleY, 0, 'f', 2));
}
