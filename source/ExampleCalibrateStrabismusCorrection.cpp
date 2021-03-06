#include "Common.h"

using namespace OVR;
using namespace glm;
using namespace gl;
enum Step {
  INTRO,
  YAW,
  PITCH,
  ROLL,
  DONE,
};

const glm::vec3 CAMERA_START = glm::vec3(0, 1, 2);

class CalibrateStrabismusCorrection : public RiftApp {
protected:
  Step step;
  glm::mat4 player;
  glm::quat ref;
  glm::quat offset;
  gl::GeometryPtr geometry;
  TextureCubeMapPtr skyboxTexture;
  glm::quat rift;
  Text::FontPtr font;

public:

  CalibrateStrabismusCorrection() : step(INTRO) {
    sensorFusion.SetGravityEnabled(true);
    sensorFusion.SetPredictionEnabled(true);
  }

  void initGl() {
    RiftApp::initGl();
    glEnable (GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GL_CHECK_ERROR;

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    GL_CHECK_ERROR;
    font = GlUtils::getDefaultFont();
    skyboxTexture = GlUtils::getCubemapTextures(
        Resource::IMAGES_SKY_TRON_XNEG_PNG);

    // Create the test geometry
    {
      Mesh mesh;
      mesh.color = Colors::white;
      mesh.addVertex(GlUtils::X_AXIS * 0.2f);
      mesh.addVertex(GlUtils::X_AXIS * 2.0f);
      mesh.addVertex(-GlUtils::X_AXIS * 0.2f);
      mesh.addVertex(-GlUtils::X_AXIS * 2.0f);
      mesh.addVertex(GlUtils::Y_AXIS * 0.2f);
      mesh.addVertex(GlUtils::Y_AXIS * 2.0f);
      mesh.addVertex(-GlUtils::Y_AXIS * 0.2f);
      mesh.addVertex(-GlUtils::Y_AXIS * 2.0f);
      mesh.fillColors();
      geometry = mesh.getGeometry(GL_LINES);
    }
  }


  void onKey(int key, int scancode, int action, int mods) {
    gl::MatrixStack & mv = gl::Stacks::modelview();

    if (GLFW_PRESS == action) switch (key) {
    case GLFW_KEY_SPACE:
      switch (step) {
      case INTRO:
        step = PITCH;
        ref = Rift::getQuaternion(sensorFusion);
        break;

      case PITCH:
        step = YAW;
        ref = Rift::getQuaternion(sensorFusion);
        break;

      case YAW:
        step = DONE;
//        step = ROLL;
//        ref = Rift::getQuaternion(sensorFusion);
//        break;
//
//      case ROLL:
//        step = DONE;

        Rift::setStrabismusCorrection(offset);
        mv.identity();
        sensorFusion.Reset();
        player = glm::inverse(glm::lookAt(CAMERA_START, glm::vec3(0, 0.5f, 0), GlUtils::Y_AXIS));
        break;

      default:
        {
          glm::vec3 euler = glm::eulerAngles(offset);
          euler *= 2.0f;
          SAY("%0.2f, %0.2f", euler.x, euler.y);
        }
        break;
      }
      return;

    case GLFW_KEY_R:
      mv.identity();
      ref = Rift::getQuaternion(sensorFusion);
      sensorFusion.Reset();
      player = glm::inverse(glm::lookAt(CAMERA_START, glm::vec3(0, 0.5f, 0), GlUtils::Y_AXIS));
      return;
    }

    if (CameraControl::instance().onKey(player, key, scancode, action, mods)) {
      return;
    }
    RiftApp::onKey(key, scancode, action, mods);
  }

  virtual void update() {
    rift = Rift::getQuaternion(sensorFusion);

    // Find the difference between the current orientation and the reference
    glm::quat distance = rift * glm::inverse(ref);
    glm::vec3 rawEuler = glm::eulerAngles(slerp(glm::quat(), distance, 0.5f));
    static glm::vec3 correctEuler;
    switch (step) {
    case PITCH:
      correctEuler.x = rawEuler.x;
      offset = glm::quat(correctEuler);
      break;

    case YAW:
      correctEuler.y = rawEuler.y;
      offset = glm::quat(correctEuler);
      break;

    case ROLL:
      correctEuler.z = rawEuler.z;
//      offset = glm::quat(correctEuler);
      break;

    case DONE:
      CameraControl::instance().applyInteraction(player);
      break;
    }
  }

  glm::mat4 getStrabismusCorrection(const PerEyeArgs & eyeArgs) {
    glm::quat strabismusCorrection = offset;
    if (eyeArgs.right) {
      strabismusCorrection = glm::inverse(offset);
    }
    return glm::mat4_cast(strabismusCorrection);
  }

  virtual void renderCalibration(const PerEyeArgs & eyeArgs) {
    gl::MatrixStack & mv = gl::Stacks::modelview();
    gl::MatrixStack & pr = gl::Stacks::projection();
    glm::vec2 cursor(-0.4f, this->eyeAspect * 0.3f);

    switch (step) {
    case INTRO:
      {
        static std::string text(
            "This utility will allow you to calibrate your per-eye offset for "
            "correcting strabismus, potentially reducing double vision and/or "
            "the need for prismatic glasses in the Rift\n\n"
            "If you normally wear prismatic glasses you should remove them for "
            "this test.\n\n"
            "Press spacebar to continue");
        mv.identity();
        gl::MatrixStack & pr = gl::Stacks::projection();
        pr.push().top() = eyeArgs.projectionOffset *
            glm::ortho(-1.0f, 1.0f, -eyeAspect, eyeAspect);
        font->renderString(text, cursor, 10.0f, 0.7f);
        pr.pop();
      }
      return;

    case PITCH:
      mv.identity();
      pr.push().top() = eyeArgs.projectionOffset *
          glm::ortho(-1.0f, 1.0f, -eyeAspect, eyeAspect);
      font->renderString("Look up and down until the lines are even\n\nPress spacebar to continue", cursor, 10.0f, 0.7f);
      pr.pop();
      mv.top() = glm::lookAt(glm::vec3(0, 5, 0),
          GlUtils::ORIGIN, GlUtils::Z_AXIS);
      break;

    case YAW:
      mv.identity();
      pr.push().top() = eyeArgs.projectionOffset * glm::ortho(-1.0f, 1.0f, -eyeAspect, eyeAspect);
      font->renderString("Move your head horizontally left and right until the lines are even", cursor, 10.0f, 0.7f);
      pr.pop();

      mv.top() = glm::lookAt(glm::vec3(5, 0, 0),
          GlUtils::ORIGIN, GlUtils::Y_AXIS);
      break;

    case ROLL:
      mv.identity();
      pr.push().top() = eyeArgs.projectionOffset * glm::ortho(-1.0f, 1.0f, -eyeAspect, eyeAspect);
      font->renderString("Move your head horizontally left and right until the lines are even", cursor, 10.0f, 0.7f);
      pr.pop();

      mv.top() = glm::lookAt(glm::vec3(0, 0, 5),
          GlUtils::ORIGIN,
          eyeArgs.left ? -GlUtils::Y_AXIS : GlUtils::Y_AXIS);
      break;
    }

    mv.push().preMultiply(getStrabismusCorrection(eyeArgs));
    GlUtils::renderGeometry(
        geometry,
        GlUtils::getProgram(
            Resource::SHADERS_SIMPLE_VS,
            Resource::SHADERS_SIMPLE_FS
        ));
    mv.pop();
  }

  virtual void renderScene(const PerEyeArgs & eyeArgs) {
    gl::Stacks::projection().top() = eyeArgs.getProjection();

    glEnable(GL_DEPTH_TEST);
    clearColor(vec3(0.05f, 0.05f, 0.05f));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (step != DONE) {
      renderCalibration(eyeArgs);
      return;
    }

    gl::MatrixStack & mv = gl::Stacks::modelview();
    gl::MatrixStack & pr = gl::Stacks::projection();

    glm::quat strabismusCorrection = offset;
    if (eyeArgs.right) {
      strabismusCorrection = glm::inverse(offset);
    }

    mv.push().top() = getStrabismusCorrection(eyeArgs) *
      (glm::inverse(player) * glm::mat4_cast(rift));
    mv.transform(eyeArgs.modelviewOffset);

    GlUtils::renderSkybox(Resource::IMAGES_SKY_TRON_XNEG_PNG);
    GlUtils::draw3dGrid();
    mv.scale(0.4f);
    mv.translate(glm::vec3(0, 0.5, 0));
    GlUtils::drawColorCube();
    mv.pop();
  }
};

RUN_OVR_APP(CalibrateStrabismusCorrection);

