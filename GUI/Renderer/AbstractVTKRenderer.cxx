#include "AbstractVTKRenderer.h"

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkGenericRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkInteractorStyleTrackballActor.h>
#include <vtkRenderer.h>
#include <vtkCommand.h>

AbstractVTKRenderer::AbstractVTKRenderer()
{
  // Create a VTK renderer
  m_Renderer = vtkSmartPointer<vtkRenderer>::New();

  // Set up a render window that uses GL commands to paint
  m_RenderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

  // Add the renderer to the window
  m_RenderWindow->AddRenderer(m_Renderer);

  // Set up the interactor
  m_Interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
  m_Interactor->SetRenderWindow(m_RenderWindow);
  m_Interactor->SetInteractorStyle(NULL);
}

void AbstractVTKRenderer::paintGL()
{
  // Update the scene
  this->Update();

  // Do the rendering but only when interactor is enabled (from QVTKWidget2)
  if(m_RenderWindow->GetInteractor() && m_RenderWindow->GetInteractor()->GetEnabled())
    m_RenderWindow->Render();
}

void AbstractVTKRenderer::initializeGL()
{
  // Is this what we should be calling?
  m_RenderWindow->OpenGLInit();
}

vtkRenderWindow *AbstractVTKRenderer::GetRenderWindow()
{
  return m_RenderWindow;
}

vtkRenderWindowInteractor *AbstractVTKRenderer::GetRenderWindowInteractor()
{
  return m_Interactor;
}

void AbstractVTKRenderer::SetInteractionStyle(AbstractVTKRenderer::InteractionStyle style)
{
  vtkSmartPointer<vtkInteractorObserver> stylePtr = NULL;
  switch(style)
    {
    case AbstractVTKRenderer::NO_INTERACTION:
      stylePtr = NULL;
      break;
    case AbstractVTKRenderer::TRACKBALL_CAMERA:
      stylePtr = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
      break;
    case AbstractVTKRenderer::TRACKBALL_ACTOR:
      stylePtr = vtkSmartPointer<vtkInteractorStyleTrackballActor>::New();
      break;
    }
  m_Interactor->SetInteractorStyle(stylePtr);
}

void AbstractVTKRenderer::SyncronizeCamera(Self *reference)
{
  // Make a copy of the camera
  m_Renderer->SetActiveCamera(reference->m_Renderer->GetActiveCamera());

  // Respond to modified events from the source interactor
  Rebroadcast(reference->m_Interactor,
              vtkCommand::ModifiedEvent, ModelUpdateEvent());

  // And vice versa
  reference->Rebroadcast(m_Interactor,
                         vtkCommand::ModifiedEvent, ModelUpdateEvent());
}

void AbstractVTKRenderer::resizeGL(int w, int h)
{
  // Pass the size to VTK
  m_RenderWindow->SetSize(w, h);
}
