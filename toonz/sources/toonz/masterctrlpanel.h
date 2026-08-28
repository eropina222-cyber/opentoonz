#ifndef MASTERCTRLPANEL_H
#define MASTERCTRLPANEL_H

#include <QDockWidget>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>

// Forward declarations
class TStageObject;
class TApp;

//=============================================================================
// MasterControllerPanel
//=============================================================================
/**
 * @brief Un QDockWidget nativo que implementa un Master Controller 2D
 * para controlar posición, rotación y escala de objetos en tiempo real.
 *
 * Características:
 * - Joystick 2D interactivo
 * - 4 poses extremas guardadas (esquinas)
 * - Interpolación bilineal automática
 * - Actualización en tiempo real del TStageObject actual
 */
class MasterControllerPanel : public QDockWidget {
  Q_OBJECT

 public:
  explicit MasterControllerPanel(QWidget *parent = nullptr);
  ~MasterControllerPanel();

 private slots:
  void onJoystickMoved(double normalizedX, double normalizedY);
  void onSavePoseTopLeft();
  void onSavePoseTopRight();
  void onSavePoseBottomLeft();
  void onSavePoseBottomRight();
  void onResetPoses();

 private:
  struct Pose {
    double x, y;           // Posición
    double rotation;       // Rotación en grados
    double scaleX, scaleY; // Escala

    Pose() : x(0), y(0), rotation(0), scaleX(1), scaleY(1) {}
  };

  // UI Components
  QWidget *m_mainWidget;
  class JoystickWidget *m_joystick;
  QLabel *m_positionLabel;
  QLabel *m_rotationLabel;
  QLabel *m_scaleLabel;
  QPushButton *m_savePoseButtons[4];
  QPushButton *m_resetButton;

  // Poses storage
  Pose m_poses[4]; // [0]=TopLeft, [1]=TopRight, [2]=BottomLeft, [3]=BottomRight
  bool m_posesSaved[4];

  // Helper functions
  void setupUI();
  void connectSignals();
  void updateFromPoses(double normalizedX, double normalizedY);
  Pose interpolateBilinear(double u, double v) const;
  void applyPoseToObject(const Pose &pose);
  void savePose(int poseIndex);
  void updateLabels(const Pose &currentPose);
};

//=============================================================================
// JoystickWidget - Custom widget for 2D joystick
//=============================================================================
class JoystickWidget : public QWidget {
  Q_OBJECT

 public:
  explicit JoystickWidget(QWidget *parent = nullptr);

  double getNormalizedX() const { return m_normalizedX; }
  double getNormalizedY() const { return m_normalizedY; }
  void setNormalized(double x, double y);

 protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

 signals:
  void joystickMoved(double normalizedX, double normalizedY);

 private:
  double m_normalizedX;
  double m_normalizedY;
  bool m_isDragging;

  void updatePosition(QMouseEvent *event);
};

#endif // MASTERCTRLPANEL_H
