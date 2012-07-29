#include <QApplication>
#include <QSettings>
#include "MainImageWindow.h"
#include "SliceViewPanel.h"
#include "IRISApplication.h"
#include "SNAPAppearanceSettings.h"
#include "CommandLineArgumentParser.h"
#include "SliceWindowCoordinator.h"
#include "SnakeWizardPanel.h"

#include "GenericSliceView.h"
#include "GenericSliceModel.h"
#include "GlobalUIModel.h"
#include "IRISImageData.h"

#include "itkEventObject.h"
#include "itkObject.h"
#include "itkCommand.h"

#include <QPlastiqueStyle>

#include "IRISMainToolbox.h"

#include "ImageIODelegates.h"

// Setup printing of stack trace on segmentation faults. This only
// works on select GNU systems
#if defined(__GNUC__) && !defined(__CYGWIN__) && !defined(__APPLE__) && !defined(sun) && !defined(WIN32)

#include <signal.h>
#include <execinfo.h>

void SegmentationFaultHandler(int sig)
{
  cerr << "*************************************" << endl;
  cerr << "ITK-SNAP: Segmentation Fault!   " << endl;
  cerr << "BACKTRACE: " << endl;
  void *array[50];
  int nsize = backtrace(array, 50);
  backtrace_symbols_fd(array, nsize, 2);
  cerr << "*************************************" << endl;
  exit(-1);
}

void SetupSignalHandlers()
{
  signal(SIGSEGV, SegmentationFaultHandler);
}

#else

void SetupSignalHandlers()
{
  // Nothing to do!
}

#endif


void usage()
{
  // Print usage info and exit
  cout << "ITK-SnAP Command Line Usage:" << endl;
  cout << "   snap [options] [main_image]" << endl;
  cout << "Options:" << endl;
  cout << "   -m FILE             : Load main image FILE (grey or RGB)" << endl;
  cout << "   -g FILE             : Load main image FILE as greyscale" << endl;
  cout << "   --rgb FILE          : Load main image FILE as RGB image" << endl;
  cout << "   -s FILE             : Load segmentation image FILE" << endl;
  cout << "   -l FILE             : Load label description file FILE" << endl;
  cout << "   -o FILE             : Load overlay image FILE (can be repeated)" << endl;
  cout << "   -c <a|c|s>          : Launch in compact single-slice mode " << endl;
  cout << "                         (axial, coronal, sagittal)" << endl;
  cout << "   -z FACTOR           : Specify initial zoom in screen pixels/mm" << endl;
}

void setupParser(CommandLineArgumentParser &parser)
{
  // Parse command line parameters
  parser.AddOption("--grey",1);
  parser.AddSynonim("--grey","-g");

  parser.AddOption("--main",1);
  parser.AddSynonim("--main","-m");

  parser.AddOption("--rgb", 1);

  parser.AddOption("--segmentation",1);
  parser.AddSynonim("--segmentation","-s");
  parser.AddSynonim("--segmentation","-seg");

  parser.AddOption("--overlay", 1);
  parser.AddSynonim("--overlay", "-o");

  parser.AddOption("--labels",1);
  parser.AddSynonim("--labels","--label");
  parser.AddSynonim("--labels","-l");

  parser.AddOption("--zoom", 1);
  parser.AddSynonim("--zoom", "-z");

  parser.AddOption("--compact", 1);
  parser.AddSynonim("--compact", "-c");

  parser.AddOption("--help", 0);
  parser.AddSynonim("--help", "-h");
}

int main(int argc, char *argv[])
{
  // Setup crash signal handlers
  SetupSignalHandlers();









  // Turn off ITK warning windows
  itk::Object::GlobalWarningDisplayOff();

  // Create an application
  QApplication app(argc, argv);
  Q_INIT_RESOURCE(SNAPResources);

  app.setStyle(new QPlastiqueStyle);
  /*
  QPalette qp = app.palette();
  qp.setColor(QPalette::Button, QColor(120,160,240));
  app.setPalette(qp);
  */

  // Create the global UI
  SmartPtr<GlobalUIModel> gui = GlobalUIModel::New();
  IRISApplication *driver = gui->GetDriver();

  // Load the user preferences
  driver->GetSystemInterface()->LoadUserPreferences();

  // Create the main window
  MainImageWindow mainwin;
  mainwin.Initialize(gui);

  /* ==========================
     PARSE COMMAND LINE OPTIONS
     ========================== */

  // Parse command line arguments
  CommandLineArgumentParser parser;
  CommandLineArgumentParseResult parseResult;
  int iTrailing = 0;

  setupParser(parser);
  if(!parser.TryParseCommandLine(argc,argv,parseResult,false,iTrailing))
    {
    cerr << "Unable to parse command line. Run " << argv[0] << " -h for help" << endl;
    return -1;
    }

  if(parseResult.IsOptionPresent("--help"))
    {
    usage();
    return 0;
    }

  // The following situations are possible for main image
  // itksnap file                       <- load as main image, detect file type
  // itksnap --main file                <- load as main image, detect file type
  // itksnap --gray file                <- load as main image, force gray
  // itksnap --rgb file                 <- load as main image, force RGB
  // itksnap --gray file1 --rgb file2   <- error
  // itksnap --gray file1 file2         <- ignore file2
  // itksnap --rgb file1 file2          <- ignore file2

  // Check validity of options for main image
  if(parseResult.IsOptionPresent("--grey") && parseResult.IsOptionPresent("--rgb"))
    {
    cerr << "Error: options --rgb and --grey are mutually exclusive." << endl;
    return -1;
    }

  if(parseResult.IsOptionPresent("--main") && parseResult.IsOptionPresent("--rgb"))
    {
    cerr << "Error: options --main and --rgb are mutually exclusive." << endl;
    return -1;
    }

  if(parseResult.IsOptionPresent("--main") && parseResult.IsOptionPresent("--grey"))
    {
    cerr << "Error: options --main and --grey are mutually exclusive." << endl;
    return -1;
    }

  // Check if a main image file is specified
  const char *fnMain = NULL;
  IRISWarningList warnings;

  IRISApplication::MainImageType main_type = IRISApplication::MAIN_ANY;
  if(parseResult.IsOptionPresent("--main"))
    {
    fnMain = parseResult.GetOptionParameter("--main");
    }
  else if(parseResult.IsOptionPresent("--grey"))
    {
    fnMain = parseResult.GetOptionParameter("--grey");
    main_type = IRISApplication::MAIN_SCALAR;
    }
  else if(parseResult.IsOptionPresent("--rgb"))
    {
    fnMain = parseResult.GetOptionParameter("--rgb");
    main_type = IRISApplication::MAIN_RGB;
    }
  else if(iTrailing < argc)
    {
    fnMain = argv[iTrailing];
    }

  // If no main, there should be no overlays, segmentation
  if(!fnMain && parseResult.IsOptionPresent("--segmentation"))
    {
    cerr << "Error: -s can not be used without -m, -g, or --rgb" << endl;
    return -1;
    }

  if(!fnMain && parseResult.IsOptionPresent("--overlay"))
    {
    cerr << "Error: -o can not be used without -m, -g, or --rgb" << endl;
    return -1;
    }

  // Load main image file
  if(fnMain)
    {
    // Update the splash screen
    // ui->UpdateSplashScreen("Loading image...");

    // Try loading the image
    try
      {
      LoadMainImageDelegate del(gui, main_type);
      gui->LoadImageNonInteractive(fnMain, del, warnings);
      }
    catch(itk::ExceptionObject &exc)
      {
      cerr << "Error loading image '" << fnMain << "'" << endl;
      cerr << "Reason: " << exc << endl;
      return -1;
      }

    // Load the segmentation if supplied
    if(parseResult.IsOptionPresent("--segmentation"))
      {
      // Get the filename
      const char *fname = parseResult.GetOptionParameter("--segmentation");

      // Update the splash screen
      // ui->UpdateSplashScreen("Loading segmentation image...");

      // Try to load the image
      try
        {
        LoadSegmentationImageDelegate del(gui);
        gui->LoadImageNonInteractive(fname, del, warnings);
        }
      catch(itk::ExceptionObject &exc)
        {
        cerr << "Error loading segmentation '" << fname << "'" << endl;
        cerr << "Reason: " << exc << endl;
        return -1;
        }
      }

    // Load overlay is supplied
    if(parseResult.IsOptionPresent("--overlay"))
      {
      // Get the filename
      const char *fname = parseResult.GetOptionParameter("--overlay");

      // Update the splash screen
      // ui->UpdateSplashScreen("Loading overlay image...");

      // Try to load the image
      try
        {
        LoadOverlayImageDelegate del(gui, IRISApplication::MAIN_ANY);
        gui->LoadImageNonInteractive(fname, del, warnings);
        }
      catch(itk::ExceptionObject &exc)
        {
        cerr << "Error loading overlay '" << fname << "'" << endl;
        cerr << "Reason: " << exc << endl;
        return -1;
        }
      }
    }

  // Load labels if supplied
  if(parseResult.IsOptionPresent("--labels"))
    {
    // Get the filename
    const char *fname = parseResult.GetOptionParameter("--labels");

    // Update the splash screen
    // ui->UpdateSplashScreen("Loading label descriptions...");

    try
      {
      // Load the label file
      driver->LoadLabelDescriptions(fname);
      }
    catch(itk::ExceptionObject &exc)
      {
      cerr << "Error reading label descriptions: " <<
        exc.GetDescription() << endl;
      }
    }

  // Set initial zoom if specified
  if(parseResult.IsOptionPresent("--zoom"))
    {
    double zoom = atof(parseResult.GetOptionParameter("--zoom"));
    if(zoom >= 0.0)
      {
      gui->GetSliceCoordinator()->SetLinkedZoom(true);
      gui->GetSliceCoordinator()->SetZoomLevelAllWindows(zoom);
      }
    else
      {
      cerr << "Invalid zoom level (" << zoom << ") specified" << endl;
      }
    }

  /*

  if(parseResult.IsOptionPresent("--compact"))
    {
    string slice = parseResult.GetOptionParameter("--compact");
    if(slice.length() == 0 || !(slice[0] == 'a' || slice[0] == 'c' || slice[0] == 's'))
      cerr << "Wrong parameter passed for '--compact', ignoring" << endl;
    else
      {
      DisplayLayout dl = ui->GetDisplayLayout();
      dl.show_main_ui = false;
      ui->SetDisplayLayout(dl);
      dl.show_panel_ui = false;
      ui->SetDisplayLayout(dl);
      dl.size = HALF_SIZE;
      ui->SetDisplayLayout(dl);
      dl.slice_config = slice[0] == 'a' ? AXIAL : (slice[0] == 'c' ? CORONAL : SAGITTAL);
      ui->SetDisplayLayout(dl);
      }
    }
    */

  // Show the panel
  mainwin.show();

  // Run application
  int rc = app.exec();

  // If everything cool, save the preferences
  if(!rc)
    driver->GetSystemInterface()->SaveUserPreferences();
}

