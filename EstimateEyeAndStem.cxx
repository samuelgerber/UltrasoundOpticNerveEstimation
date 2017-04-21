#define DEBUG


#define STEM_THRESHOLD_DISTANCE  75


#include "itkImage.h"
#include "itkImageRegistrationMethodv4.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkMeanSquaresImageToImageMetricv4.h"
#include "itkLBFGSOptimizerv4.h"
#include "itkResampleImageFilter.h"
#include "itkApproximateSignedDistanceMapImageFilter.h"
#include "itkCastImageFilter.h"
#include <itkImageMaskSpatialObject.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkBinaryMorphologicalOpeningImageFilter.h>
#include <itkGrayscaleMorphologicalOpeningImageFilter.h>
#include "itkBinaryBallStructuringElement.h"
#include "itkMinimumMaximumImageCalculator.h"
#include "itkLabelMapToLabelImageFilter.h"
#include "itkLabelOverlayImageFilter.h"
#include "itkBinaryImageToLabelMapFilter.h"
#include "itkRGBPixel.h"
#include "itkRegionOfInterestImageFilter.h"
#include <itkSimilarity2DTransform.h>

#include "itkImageMaskSpatialObject.h"
#include "itkEllipseSpatialObject.h"
#include "itkSpatialObjectToImageFilter.h"

#include <tclap/CmdLine.h>
#include <algorithm>

#include "ImageIO.h"
#include "ITKFilterFunctions.h"
#include "itkImageRegionIterator.h"

typedef  float  PixelType;
typedef itk::Image< PixelType, 2 >  ImageType;
typedef itk::Image<unsigned char, 2>  UnsignedCharImageType;

typedef itk::CastImageFilter< ImageType, UnsignedCharImageType > CastFilter;
typedef itk::ApproximateSignedDistanceMapImageFilter< UnsignedCharImageType, ImageType  > SignedDistanceFilter;
 
typedef itk::BinaryBallStructuringElement<ImageType::PixelType, ImageType::ImageDimension> StructuringElementType;
typedef itk::BinaryMorphologicalClosingImageFilter<ImageType, ImageType, StructuringElementType> ClosingFilter;
typedef itk::BinaryMorphologicalOpeningImageFilter<ImageType, ImageType, StructuringElementType> OpeningFilter;
typedef itk::GrayscaleMorphologicalOpeningImageFilter<ImageType, ImageType, StructuringElementType> GrayOpeningFilter;

typedef itk::MinimumMaximumImageCalculator <ImageType> ImageCalculatorFilterType;

//typedef itk::ExtractImageFilter< ImageType, ImageType > ExtractFilter;
typedef itk::RegionOfInterestImageFilter< ImageType, ImageType > ExtractFilter;
typedef itk::RegionOfInterestImageFilter< UnsignedCharImageType, UnsignedCharImageType > ExtractFilter2;


//Overlay
typedef itk::RGBPixel<unsigned char> RGBPixelType;
typedef itk::Image<RGBPixelType> RGBImageType;
typedef itk::LabelOverlayImageFilter<ImageType, UnsignedCharImageType, RGBImageType> LabelOverlayImageFilterType;
typedef itk::BinaryImageToLabelMapFilter<UnsignedCharImageType> BinaryImageToLabelMapFilterType;
typedef itk::LabelMapToLabelImageFilter<BinaryImageToLabelMapFilterType::OutputImageType, UnsignedCharImageType> LabelMapToLabelImageFilterType;


//registration
//typedef itk::GradientDescentOptimizer       OptimizerType;
//typedef itk::ConjugateGradientOptimizer       OptimizerType;
typedef itk::LBFGSOptimizerv4       OptimizerType;

typedef itk::MeanSquaresImageToImageMetricv4< ImageType, ImageType >  MetricType;
typedef itk::LinearInterpolateImageFunction< ImageType, double >    InterpolatorType;
typedef itk::ImageRegistrationMethodv4< ImageType, ImageType >    RegistrationType;

typedef itk::Similarity2DTransform< double >     SimilarityTransformType;
typedef itk::AffineTransform< double, 2 >     AffineTransformType;

typedef itk::ResampleImageFilter< ImageType, ImageType >    ResampleFilterType;

//Mask image spatical object 
typedef itk::ImageMaskSpatialObject< 2 >   MaskType;

//Elipse object  
typedef itk::EllipseSpatialObject< 2 >   EllipseType;
typedef itk::SpatialObjectToImageFilter< EllipseType, ImageType >   SpatialObjectToImageFilterType;
typedef EllipseType::TransformType EllipseTransformType;







//Storage for eye and stem location and sizes
struct Eye{
  ImageType::IndexType initialCenterIndex;
  ImageType::PointType initialCenter;
  ImageType::IndexType centerIndex;
  ImageType::PointType center;
  double initialRadius;
  double minor;
  double major;

  double initialRadiusX;
  double initialRadiusY;
  
  ImageType::Pointer aligned;
};



struct Stem{
  ImageType::IndexType initialCenterIndex;
  ImageType::PointType initialCenter;
  ImageType::IndexType centerIndex;
  ImageType::PointType center;
  double initialWidth;
  double width;

  ImageType::Pointer aligned;
  ImageType::RegionType originalImageRegion;
};




//Helper function
std::string catStrings(std::string s1, std::string s2){
  std::stringstream out;
  out << s1 << s2;
  return out.str();
};



//Create ellipse image
ImageType::Pointer CreateEllipseImage( ImageType::SpacingType spacing, 
		                       ImageType::SizeType size, 
				       ImageType::PointType origin,
				       ImageType::PointType center,
				       double r, double outside = 100, double inside = 0){
 
  SpatialObjectToImageFilterType::Pointer imageFilter =
    SpatialObjectToImageFilterType::New();
  
  //origin[0] -= 0.5 * size[0] * spacing[0];
  //origin[1] -= 0.5 * size[1] * spacing[1];
  //size[0] *= 2;
  //size[1] *= 2;
  imageFilter->SetSize( size );
  imageFilter->SetOrigin( origin );
  imageFilter->SetSpacing( spacing );
 
  EllipseType::Pointer ellipse   = EllipseType::New();
  EllipseType::ArrayType radiusArray;
  radiusArray[0] = r;
  radiusArray[1] = r;
  ellipse->SetRadius(radiusArray);
 
  EllipseTransformType::Pointer transform = EllipseTransformType::New();
  transform->SetIdentity();
  EllipseTransformType::OutputVectorType  translation;
  translation[ 0 ] =  center[0];
  translation[ 1 ] =  center[1];
  transform->Translate( translation, false );
  ellipse->SetObjectToParentTransform( transform );
 
  imageFilter->SetInput(ellipse);
  ellipse->SetDefaultInsideValue( inside );
  ellipse->SetDefaultOutsideValue( outside );
  imageFilter->SetUseObjectValue( true );
  imageFilter->SetOutsideValue( outside );
  imageFilter->Update();
 
  return imageFilter->GetOutput();
}







//Fit an ellipse to an eye ultrasound image
Eye fitEye(ImageType::Pointer inputImage, const std::string &prefix){

  std::cout << "--- Fitting Eye ---" << std::endl << std::endl;
  
  Eye eye;

  ////
  //1. PReprcess input image
  ////
  ImageType::Pointer image = ITKFilterFunctions<ImageType>::Rescale(inputImage, 0, 100);

  ImageType::SpacingType imageSpacing = image->GetSpacing();
  ImageType::RegionType imageRegion = image->GetLargestPossibleRegion();
  ImageType::SizeType imageSize = imageRegion.GetSize();
  ImageType::PointType imageOrigin = image->GetOrigin();

  //add white border
  ITKFilterFunctions<ImageType>::AddBorder(image, 30);



  std::cout << "Origin, spacing, size input image" << std::endl;
  std::cout << imageOrigin << std::endl;
  std::cout << imageSpacing << std::endl;
  std::cout << imageSize << std::endl;

  ITKFilterFunctions<ImageType>::SigmaArrayType sigma;
  sigma[0] = 10 * imageSpacing[0]; 
  sigma[1] = 10* imageSpacing[1]; 
  
  image = ITKFilterFunctions<ImageType>::GaussSmooth(image, sigma);


  ////
  //2. Find center and size of eye using a distance transform of binarized image
  ////
  //TODO should use binarize filter
  image = ITKFilterFunctions<ImageType>::ThresholdAbove(image, 25, 100);
  image = ITKFilterFunctions<ImageType>::ThresholdBelow(image, 25,  0);

  
  StructuringElementType structuringElement;
  structuringElement.SetRadius( 50 );
  structuringElement.CreateStructuringElement();
  ClosingFilter::Pointer closingFilter = ClosingFilter::New();
  closingFilter->SetInput(image);
  closingFilter->SetKernel(structuringElement);
  closingFilter->SetForegroundValue(100.0);
  closingFilter->Update();
  image = closingFilter->GetOutput();
  
  CastFilter::Pointer maskCastFilter = CastFilter::New();
  maskCastFilter->SetInput( image );
  maskCastFilter->Update();
  SignedDistanceFilter::Pointer signedDistanceFilter = SignedDistanceFilter::New();
  signedDistanceFilter->SetInput( maskCastFilter->GetOutput() );
  signedDistanceFilter->SetInsideValue(100);
  signedDistanceFilter->SetOutsideValue(0);
  signedDistanceFilter->Update();
  ImageType::Pointer imageDistance = signedDistanceFilter->GetOutput();

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage(imageDistance, catStrings(prefix, "-eye-distance.tif") );
#endif

  //Compute max of distance transfrom 
  ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New ();
  imageCalculatorFilter->SetImage( imageDistance );
  imageCalculatorFilter->Compute();
  
  eye.initialRadius = imageCalculatorFilter->GetMaximum() ;
  eye.initialCenterIndex = imageCalculatorFilter->GetIndexOfMaximum();
  image->TransformIndexToPhysicalPoint(eye.initialCenterIndex, eye.initialCenter);
  
  std::cout << "Eye inital center: " << eye.initialCenterIndex << std::endl;
  std::cout << "Eye initial radius: "<< eye.initialRadius << std::endl;

  //Compute vertical distance to eye border
  ImageType::SizeType yRegionSize;
  yRegionSize[0] = 20;
  yRegionSize[1] = imageSize[1];
  ImageType::IndexType yRegionIndex;
  yRegionIndex[0] =  eye.initialCenterIndex[0] - 10;
  yRegionIndex[1] =  0;
  ImageType::RegionType yRegion(yRegionIndex, yRegionSize);
  
  ExtractFilter2::Pointer extractFilterY = ExtractFilter2::New();
  extractFilterY->SetRegionOfInterest(yRegion);
  extractFilterY->SetInput( maskCastFilter->GetOutput() );
  extractFilterY->Update();

  SignedDistanceFilter::Pointer signedDistanceY = SignedDistanceFilter::New();
  signedDistanceY->SetInput( extractFilterY->GetOutput() );
  signedDistanceY->SetInsideValue(100);
  signedDistanceY->SetOutsideValue(0);
  //signedDistanceY->GetOutput()->SetRequestedRegion( yRegion );
  signedDistanceY->Update();
  ImageType::Pointer imageDistanceY = signedDistanceY->GetOutput();

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage(imageDistanceY, catStrings(prefix, "-eye-ydistance.tif") );
#endif

  ImageCalculatorFilterType::Pointer imageCalculatorY = ImageCalculatorFilterType::New ();
  imageCalculatorY->SetImage( imageDistanceY );
  imageCalculatorY->Compute();
  
  eye.initialRadiusY = imageCalculatorY->GetMaximum() ;

  std::cout << "Eye initial radiusY: "<< eye.initialRadiusY << std::endl;
  
  
  //Compute horizontal distance to eye border
  ImageType::SizeType xRegionSize;
  xRegionSize[0] = imageSize[0];
  xRegionSize[1] = 20;
  ImageType::IndexType xRegionIndex;
  xRegionIndex[0] =  0;
  xRegionIndex[1] =  eye.initialCenterIndex[1] - 10;
  ImageType::RegionType xRegion(xRegionIndex, xRegionSize);
    
  ExtractFilter2::Pointer extractFilterX = ExtractFilter2::New();
  extractFilterX->SetRegionOfInterest(xRegion);
  extractFilterX->SetInput( maskCastFilter->GetOutput() );
  extractFilterX->Update();

  SignedDistanceFilter::Pointer signedDistanceX = SignedDistanceFilter::New();
  signedDistanceX->SetInput( extractFilterX->GetOutput() );
  signedDistanceX->SetInsideValue(100);
  signedDistanceX->SetOutsideValue(0);
  //signedDistanceX->GetOutput()->SetRequestedRegion( xRegion );
  signedDistanceX->Update();
  ImageType::Pointer imageDistanceX = signedDistanceX->GetOutput();

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage(imageDistanceX, catStrings(prefix, "-eye-xdistance.tif") );
#endif

  ImageCalculatorFilterType::Pointer imageCalculatorX = ImageCalculatorFilterType::New ();
  imageCalculatorX->SetImage( imageDistanceX );
  imageCalculatorX->Compute();
  
  eye.initialRadiusX = imageCalculatorX->GetMaximum() ;

  std::cout << "Eye initial radiusX: "<< eye.initialRadiusX << std::endl;


  //smooth thresholded image fro registration
  sigma[0] = 16 * imageSpacing[0]; 
  sigma[1] = 16 * imageSpacing[1]; 
  ImageType::Pointer imageSmooth = ITKFilterFunctions<ImageType>::GaussSmooth(image, sigma);

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage(imageSmooth, catStrings(prefix, "-eye-smooth.tif") );
#endif

  ////
  //4. Create ellipse image
  ////
  double mean = 100;//statisticsFilter->GetMean();   
  double r1 = eye.initialRadius * 1;
  double r2 = r1 * 1.1;
  ImageType::Pointer e1 = CreateEllipseImage( imageSpacing, imageSize, imageOrigin, eye.initialCenter, r1, mean);
  ImageType::Pointer e2 = CreateEllipseImage( imageSpacing, imageSize, imageOrigin, eye.initialCenter, r2, mean);
  ImageType::Pointer ellipse = ITKFilterFunctions<ImageType>::Subtract(e1, e2);

  sigma[0] = 16 * imageSpacing[0]; 
  sigma[1] = 16 * imageSpacing[1];
  ellipse = ITKFilterFunctions<ImageType>::GaussSmooth(ellipse, sigma);
  ellipse = ITKFilterFunctions<ImageType>::Rescale(ellipse, 0, 1);

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( ellipse, catStrings(prefix, "-eye-moving.tif") );
#endif 

  std::cout << "Origin, spacing, size ellipse image" << std::endl;
  std::cout << ellipse->GetOrigin() << std::endl;
  std::cout << ellipse->GetSpacing() << std::endl;
  std::cout << ellipse->GetLargestPossibleRegion().GetSize() << std::endl;



  ////
  //5. Setup registration
  ////
  AffineTransformType::Pointer transform = AffineTransformType::New();
  transform->SetCenter(eye.initialCenter);


  MetricType::Pointer         metric        = MetricType::New();
  OptimizerType::Pointer      optimizer       = OptimizerType::New();
  InterpolatorType::Pointer   movingInterpolator  = InterpolatorType::New();
  InterpolatorType::Pointer   fixedInterpolator  = InterpolatorType::New();
  RegistrationType::Pointer   registration  = RegistrationType::New();


  optimizer->SetGradientConvergenceTolerance( 0.000001 );
  optimizer->SetLineSearchAccuracy( 0.5 );
  optimizer->SetDefaultStepLength( 0.00001 );
  optimizer->TraceOn();
  optimizer->SetMaximumNumberOfFunctionEvaluations( 20000 );

  
  OptimizerType::ScalesType scales( transform->GetNumberOfParameters() );
  scales[0] = 1.0;
  scales[1] = 1.0;
  scales[2] = 1.0; 
  scales[3] = 1.0; 
  scales[4] = 1.0; 
  scales[5] = 1.0;  
  optimizer->SetScales( scales );


  metric->SetMovingInterpolator( movingInterpolator );
  metric->SetFixedInterpolator( fixedInterpolator );  
 
  ImageType::Pointer ellipseMask = CreateEllipseImage( imageSpacing, imageSize, imageOrigin, 
		                                       eye.initialCenter, (r1+r2)/2.0, 0, 100 ); 
  
  CastFilter::Pointer castFilter = CastFilter::New();
  castFilter->SetInput( ellipseMask );
  castFilter->Update();
  MaskType::Pointer  spatialObjectMask = MaskType::New();
  spatialObjectMask->SetImage( castFilter->GetOutput() );
  metric->SetFixedImageMask( spatialObjectMask );
	  
#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( ellipseMask, catStrings(prefix, "-eye-mask.tif")  );
#endif

  registration->SetMetric(        metric        );
  registration->SetOptimizer(     optimizer     );

  registration->SetMovingImage(    imageSmooth    );
  registration->SetFixedImage(   ellipse   );

  std::cout <<  transform->GetParameters()  << std::endl;
  std::cout <<  transform->GetCenter()  << std::endl;
  registration->SetInitialTransform( transform );
    
  RegistrationType::ShrinkFactorsArrayType shrinkFactorsPerLevel;
  shrinkFactorsPerLevel.SetSize( 1 );
  //shrinkFactorsPerLevel[0] = 2;
  //shrinkFactorsPerLevel[1] = 1;
  shrinkFactorsPerLevel[0] = 1;

  RegistrationType::SmoothingSigmasArrayType smoothingSigmasPerLevel;
  smoothingSigmasPerLevel.SetSize( 1 );
  //smoothingSigmasPerLevel[0] = 1;
  //smoothingSigmasPerLevel[1] = 0;
  smoothingSigmasPerLevel[0] = 0;

  registration->SetNumberOfLevels ( 1 );
  registration->SetSmoothingSigmasPerLevel( smoothingSigmasPerLevel );
  registration->SetShrinkFactorsPerLevel( shrinkFactorsPerLevel );
  
  //Do registration
  try{
	  registration->SetNumberOfThreads(1);
	  registration->Update();
  }
  catch( itk::ExceptionObject & err ){
	  std::cerr << "ExceptionObject caught !" << std::endl;
	  std::cerr << err << std::endl;
	  //return EXIT_FAILURE;
  }


  const double bestValue = optimizer->GetValue();
  std::cout << "Result = " << std::endl;
  std::cout << " Metric value  = " << bestValue          << std::endl;

  std::cout << "Optimized transform paramters:" << std::endl;
  std::cout <<  registration->GetTransform()->GetParameters()  << std::endl;
  std::cout <<  transform->GetCenter()  << std::endl;



  AffineTransformType::Pointer inverse = AffineTransformType::New();
  transform->GetInverse( inverse );

  ////
  //6. Create registered ellipse image
  ////
  ResampleFilterType::Pointer resampler = ResampleFilterType::New();
  resampler->SetInput( ellipse );
  resampler->SetTransform( inverse );
  resampler->SetSize( imageSize );
  resampler->SetOutputOrigin(  image->GetOrigin() );
  resampler->SetOutputSpacing( imageSpacing );
  resampler->SetOutputDirection( image->GetDirection() );
  resampler->SetDefaultPixelValue( 0 );
  resampler->Update();
  ImageType::Pointer moved = resampler->GetOutput();

  eye.aligned = moved;

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( moved, catStrings(prefix, "-eye-registred.tif")  );


  ImageType::Pointer ellipseThres = ITKFilterFunctions<ImageType>::ThresholdAbove(moved, 5, 255);
  CastFilter::Pointer teCast = CastFilter::New();
  teCast->SetInput( ellipseThres );

  BinaryImageToLabelMapFilterType::Pointer binaryImageToLabelMapFilter = BinaryImageToLabelMapFilterType::New();
  binaryImageToLabelMapFilter->SetInput( teCast->GetOutput() );
  binaryImageToLabelMapFilter->Update();
 
  LabelMapToLabelImageFilterType::Pointer labelMapToLabelImageFilter = LabelMapToLabelImageFilterType::New();
  labelMapToLabelImageFilter->SetInput(binaryImageToLabelMapFilter->GetOutput());
  labelMapToLabelImageFilter->Update();
 
  ImageType::Pointer imageRescale = ITKFilterFunctions<ImageType>::Rescale(inputImage, 0, 255);
  LabelOverlayImageFilterType::Pointer labelOverlayImageFilter = LabelOverlayImageFilterType::New();
  labelOverlayImageFilter->SetInput( imageRescale );
  labelOverlayImageFilter->SetLabelImage(labelMapToLabelImageFilter->GetOutput());
  labelOverlayImageFilter->SetOpacity(.5);
  labelOverlayImageFilter->Update();
  
  ImageIO<RGBImageType>::WriteImage( labelOverlayImageFilter->GetOutput(), catStrings(prefix, "-eye-overlay.png") );
#endif


  ////
  // 7. Transfrom initial estimates
  ////
  AffineTransformType::InputPointType tCenter;
  tCenter[0] = eye.initialCenter[0];
  tCenter[1] = eye.initialCenter[1];
  
  AffineTransformType::InputVectorType tX;
  tX[0] = eye.initialRadius;
  tX[1] = 0;

  AffineTransformType::InputVectorType tY;
  tY[0] = 0;
  tY[1] = eye.initialRadius;
  
  eye.center = inverse->TransformPoint(tCenter);
  image->TransformPhysicalPointToIndex(eye.center, eye.centerIndex);

  AffineTransformType::OutputVectorType tXO = inverse->TransformVector(tX, tCenter);
  AffineTransformType::OutputVectorType tYO =inverse->TransformVector(tY, tCenter);

  eye.minor =  sqrt(tXO[0]*tXO[0] + tXO[1]*tXO[1]); 
  eye.major =  sqrt(tYO[0]*tYO[0] + tYO[1]*tYO[1]); 

  if(eye.major < eye.minor){
    std::swap(eye.minor, eye.major);
  }

  std::cout << "Eye center: " << eye.centerIndex << std::endl;
  std::cout << "Eye minor: "  << eye.minor << std::endl;
  std::cout << "Eye major: "  << eye.major << std::endl;




  std::cout << "--- Done Fitting Eye ---" << std::endl << std::endl;

  return eye;
};




//Fit stem based on eye location and size
//Fit is done using an idealized stem image and fit it 
//to the region the stem is expected to be located based on 
//the eye information.
Stem fitStem(ImageType::Pointer inputImage, Eye &eye, const std::string &prefix){
  std::cout << "--- Fit stem ---" << std::endl << std::endl;
  
  
  Stem stem;
  
  ImageType::SpacingType imageSpacing = inputImage->GetSpacing();
  ImageType::RegionType imageRegion = inputImage->GetLargestPossibleRegion();
  ImageType::SizeType imageSize = imageRegion.GetSize();
  ImageType::PointType imageOrigin = inputImage->GetOrigin();

  ////
  //3. Extract cranial stem region of interest and process 
  //   to yield a binary image of the stem. 
  ////
  ImageType::IndexType desiredStart;
  desiredStart[0] = eye.center[0] - 0.9 * eye.initialRadius;
  desiredStart[1] = eye.initialCenter[1] + 1.1 * eye.initialRadiusY ;
 
  ImageType::SizeType desiredSize;
  desiredSize[0] = 1.8 * eye.initialRadius;
  desiredSize[1] = 0.8 * eye.initialRadius;
  if(desiredStart[1] + desiredSize[1] > imageSize[1] ){
    desiredSize[1] = imageSize[1] - desiredStart[1];
  }
  if(desiredStart[0] < 0 ){
    desiredStart[0] = 0;
  }
  if(desiredStart[0] + desiredSize[0] > imageSize[0] ){
    desiredSize[0] = imageSize[0] - desiredStart[0];
  }

 
  ImageType::RegionType desiredRegion(desiredStart, desiredSize);
  stem.originalImageRegion = desiredRegion;

  ExtractFilter::Pointer extractFilter = ExtractFilter::New();
  //extractFilter->SetExtractionRegion(desiredRegion);
  extractFilter->SetRegionOfInterest(desiredRegion);
  extractFilter->SetInput( inputImage );
  //extractFilter->SetDirectionCollapseToIdentity(); 
  extractFilter->Update();
  ImageType::Pointer stemImageOrig = extractFilter->GetOutput();
  


  ImageType::RegionType stemRegion = stemImageOrig->GetLargestPossibleRegion();
  ImageType::SizeType stemSize = stemRegion.GetSize();
  ImageType::PointType stemOrigin = stemImageOrig->GetOrigin();
  ImageType::SpacingType stemSpacing = stemImageOrig->GetSpacing();

  std::cout << "Origin, spacing, size and index of stem image" << std::endl;
  std::cout << stemOrigin << std::endl;
  std::cout << stemSpacing << std::endl;
  std::cout << stemSize << std::endl;
  std::cout << stemRegion.GetIndex() << std::endl;






  ////
  //4. Find center and approximate width of stem using a distance transform 
  ////
  ITKFilterFunctions<ImageType>::SigmaArrayType sigma;
  sigma[0] = 1.5 * stemSpacing[0]; 
  sigma[1] = 20 * stemSpacing[1]; 
  //sigma[1] = stemSize[1]/12.0 * stemSpacing[1]; 
  ImageType::Pointer stemImage = ITKFilterFunctions<ImageType>::GaussSmooth( stemImageOrig, sigma);
 
  //Reascle indiviudal rows 
  ITKFilterFunctions<ImageType>::RescaleRows(stemImage);


  stemImage = ITKFilterFunctions<ImageType>::Rescale(stemImage, 0, 100);

  
#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( stemImage, catStrings(prefix, "-stem.tif") );
#endif 
   
  //TODO: Use binarize filter
  ImageType::Pointer stemImageB = 
	       ITKFilterFunctions<ImageType>::ThresholdAbove( stemImage,  STEM_THRESHOLD_DISTANCE, 100 );
  stemImageB = ITKFilterFunctions<ImageType>::ThresholdBelow( stemImageB, STEM_THRESHOLD_DISTANCE,   0 );

  //sigma[0] = 5.0 * stemSpacing[0]; 
  //sigma[1] = 5.0 * stemSpacing[1]; 
  //stemImage = ITKFilterFunctions<ImageType>::GaussSmooth(stemImage, sigma);


#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( stemImageB, catStrings(prefix, "-stem-sd-thres.tif") );
#endif

  StructuringElementType structuringElement;
  structuringElement.SetRadius( 30 );
  structuringElement.CreateStructuringElement();
  OpeningFilter::Pointer openingFilter = OpeningFilter::New();
  openingFilter->SetInput(stemImageB);
  openingFilter->SetKernel(structuringElement);
  openingFilter->SetForegroundValue(100.0);
  openingFilter->Update();
  stemImageB = openingFilter->GetOutput();
 
#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( stemImageB, catStrings(prefix, "-stem-morpho.tif") );
#endif

  CastFilter::Pointer stemCastFilter = CastFilter::New();
  stemCastFilter->SetInput( stemImageB );
  stemCastFilter->Update();
  UnsignedCharImageType::Pointer stemImage2 = stemCastFilter->GetOutput();

  ITKFilterFunctions<UnsignedCharImageType>::AddVerticalBorder(stemImage2, 20);
  ITKFilterFunctions<UnsignedCharImageType>::AddHorizontalBorder(stemImage2, 5);

  SignedDistanceFilter::Pointer stemDistanceFilter = SignedDistanceFilter::New();
  stemDistanceFilter->SetInput( stemImage2 );
  stemDistanceFilter->SetInsideValue(100);
  stemDistanceFilter->SetOutsideValue(0);
  stemDistanceFilter->Update();
  ImageType::Pointer stemDistance = stemDistanceFilter->GetOutput();

  //This will distort the inital stem width estimate
  //sigma[0] = 10.0 * stemSpacing[0]; 
  //sigma[1] = 10.0 * stemSpacing[1]; 
  //stemDistance = ITKFilterFunctions<ImageType>::GaussSmooth(stemDistance, sigma);

  
#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( stemDistance, catStrings(prefix, "-stem-distance.tif") );
#endif
 
  //Compute max of distance transfrom 
  ImageCalculatorFilterType::Pointer stemCalculatorFilter  = ImageCalculatorFilterType::New ();
  stemCalculatorFilter->SetImage( stemDistance );
  stemCalculatorFilter->Compute();
  
  stem.initialWidth = stemCalculatorFilter->GetMaximum() ;
  stem.initialCenterIndex = stemCalculatorFilter->GetIndexOfMaximum();
  stemImage->TransformIndexToPhysicalPoint(stem.initialCenterIndex, stem.initialCenter);

  std::cout << "Approximate width of stem: " <<  2 * stem.initialWidth << std::endl;
  std::cout << "Approximate stem center: " << stem.initialCenterIndex << std::endl;
  

  //Rescale rows left and righ of the approximate center to 0 - 100
  float centerIntensity = stemImage->GetPixel( stem.initialCenterIndex );
  std::cout << "Approximate stem center intensity: " << centerIntensity << std::endl;
  for(int i=0; i<stemSize[1]; i++){
    ImageType::IndexType index;
    index[1] = i;
    float maxIntensityLeft = 0;
    for(int j=0; j<stem.initialCenterIndex[0]; j++){
      index[0] = j;
      maxIntensityLeft = std::max( stemImage->GetPixel(index), maxIntensityLeft );
    }
    for(int j=0; j<stem.initialCenterIndex[0]; j++){
      index[0] = j;
      float value = ( stemImage->GetPixel(index) - centerIntensity ) / 
		  ( maxIntensityLeft - centerIntensity )  ;
      value = std::max(0.f, value)*100;
      value = std::min(100.f, value);
      stemImage->SetPixel(index, value );
    }
    
    float maxIntensityRight = 0;
    for(int j=stem.initialCenterIndex[0]; j<stemSize[0]; j++){
      index[0] = j;
      maxIntensityRight = std::max( stemImage->GetPixel(index), maxIntensityRight );
    }
    for(int j=stem.initialCenterIndex[0]; j<stemSize[0]; j++){
      index[0] = j;
      float value = ( stemImage->GetPixel(index) - centerIntensity ) / 
		  ( maxIntensityRight - centerIntensity ) ;
      value = std::max(0.f, value) * 100;
      value = std::min(100.f, value);
      stemImage->SetPixel(index, value  );
    }
    

  }

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( stemImage, catStrings(prefix, "-stem-scaled.tif") );
#endif


  //Not needed with above scaled image
  /*
  float tb = 70;
  std::cout << "Stem threshold: " << tb << std::endl;
  stemImage = ITKFilterFunctions<ImageType>::ThresholdAbove(stemImage, tb, 100);
  stemImage = ITKFilterFunctions<ImageType>::ThresholdBelow(stemImage, tb, 0);
    
  StructuringElementType structuringElement2;
  structuringElement2.SetRadius( 15 );
  structuringElement2.CreateStructuringElement();
  OpeningFilter::Pointer openingFilter2 = OpeningFilter::New();
  openingFilter2->SetInput(stemImage);
  openingFilter2->SetKernel(structuringElement2);
  openingFilter2->SetForegroundValue(100.0);
  openingFilter2->Update();
  stemImage = openingFilter2->GetOutput();
  */
  /*
  sigma[0] = 2.0 * stemSpacing[0]; 
  sigma[1] = 2.0 * stemSpacing[1]; 
  stemImage = ITKFilterFunctions<ImageType>::GaussSmooth(stemImage, sigma);
  */



#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( stemImage, catStrings(prefix, "-stem-thres.tif") );
#endif

  ////
  //5. Create artifical stem image to fit to region of interest
  ////
  ImageType::Pointer moving = ImageType::New();
  moving->SetRegions(stemRegion);
  moving->Allocate();
  moving->FillBuffer( 0.0 );
  moving->SetSpacing(stemSpacing);
  moving->SetOrigin(stemOrigin);

  UnsignedCharImageType::Pointer movingMask = UnsignedCharImageType::New();
  movingMask->SetRegions(stemRegion);
  movingMask->Allocate();
  movingMask->FillBuffer( itk::NumericTraits< unsigned char >::Zero);
  movingMask->SetSpacing(stemSpacing);
  movingMask->SetOrigin(stemOrigin);

  int stemYStart   = stemSize[1] * 0.2;
  int stemXStart1  = stem.initialCenterIndex[0] - 1.9 * stem.initialWidth / stemSpacing[0];
  int stemXEnd1    = stem.initialCenterIndex[0] - 1 * stem.initialWidth / stemSpacing[0];
  int stemXStart2  = stem.initialCenterIndex[0] + 1 * stem.initialWidth / stemSpacing[0];
  int stemXEnd2    = stem.initialCenterIndex[0] + 1.9 * stem.initialWidth / stemSpacing[0];


  if(stemXStart1 < 0){
    stemXStart1 = 0;
  }
  if( stemXEnd2 >= stemSize[0] ){
    stemXEnd2 = stemSize[0];
  }
  if(stemXEnd2 < stemXStart2){ 
    std::cout << "Failed to locate stem" << std::endl;
    return stem;
  }

  for(int i=stemYStart; i<stemSize[1]; i++){
     ImageType::IndexType index;
     index[1] = i;
     for(int j=stemXStart1; j<stemXEnd2; j++){
       index[0]=j;
       movingMask->SetPixel(index, 255);
     }

     for(int j=stemXStart1; j<stemXEnd1; j++){
       index[0]=j;
       moving->SetPixel(index, 100.0);
     }
     for(int j=stemXStart2; j<stemXEnd2; j++){
       index[0]=j;
       moving->SetPixel(index, 100.0);
     }    
  }

#ifdef DEBUG
  ImageIO<UnsignedCharImageType>::WriteImage( movingMask, catStrings(prefix, "-stem-mask.tif") );
#endif

  sigma[0] = 2.5 * stemSpacing[0]; 
  sigma[1] = 2,5 * stemSpacing[1]; 
  moving = ITKFilterFunctions<ImageType>::GaussSmooth(moving, sigma);
  
#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( moving, catStrings(prefix, "-stem-moving.tif") );
#endif




  ////
  //6. Registration of artifical stem image to threhsold stem image
  ////
  SimilarityTransformType::Pointer transform = SimilarityTransformType::New();
  transform->SetCenter( stem.initialCenter );


  MetricType::Pointer         metric        = MetricType::New();
  OptimizerType::Pointer      optimizer       = OptimizerType::New();
  InterpolatorType::Pointer   movingInterpolator  = InterpolatorType::New();
  InterpolatorType::Pointer   fixedInterpolator  = InterpolatorType::New();
  RegistrationType::Pointer   registration  = RegistrationType::New();

  optimizer->SetGradientConvergenceTolerance( 0.000001 );
  optimizer->SetLineSearchAccuracy( 0.5 );
  optimizer->SetDefaultStepLength( 0.00001 );
  optimizer->TraceOn();
  optimizer->SetMaximumNumberOfFunctionEvaluations( 20000 );

  
  //Using a Quasi-Newton method, amke sure scales are set to identity to no destory the approximation of the Hessian
  std::cout << transform->GetNumberOfParameters() << std::endl;
  OptimizerType::ScalesType scales( transform->GetNumberOfParameters() );
  scales[0] = 1.0;
  scales[1] = 1.0;
  scales[2] = 1.0; 
  scales[3] = 1.0; 
  //scales[4] = 1.0; 
  //scales[5] = 1.0;  
  optimizer->SetScales( scales );


  metric->SetMovingInterpolator( movingInterpolator );
  metric->SetFixedInterpolator( fixedInterpolator );  
 
  MaskType::Pointer  spatialObjectMask = MaskType::New();
  spatialObjectMask->SetImage( movingMask );
  metric->SetFixedImageMask( spatialObjectMask );


  registration->SetMetric(        metric        );
  registration->SetOptimizer(     optimizer     );
  //registration->SetFixedImage(    imageSmooth    );
  //registration->SetMovingImage(   ellipse   );
  registration->SetMovingImage(    stemImage    );
  registration->SetFixedImage(   moving  );


  std::cout << "Transform parameters: " << std::endl;
  std::cout <<  transform->GetParameters()  << std::endl;
  std::cout <<  transform->GetCenter()  << std::endl;
  registration->SetInitialTransform( transform );
    
  RegistrationType::ShrinkFactorsArrayType shrinkFactorsPerLevel;
  shrinkFactorsPerLevel.SetSize( 1 );
  //shrinkFactorsPerLevel[0] = 1;
  //shrinkFactorsPerLevel[1] = 1;
  shrinkFactorsPerLevel[0] = 1;

  RegistrationType::SmoothingSigmasArrayType smoothingSigmasPerLevel;
  smoothingSigmasPerLevel.SetSize( 1 );
  //smoothingSigmasPerLevel[0] = stem.initialWidth / 2.0 * imageSpacing[0];
  //smoothingSigmasPerLevel[1] = 0;
  smoothingSigmasPerLevel[0] = 0;

  registration->SetNumberOfLevels ( 1 );
  registration->SetSmoothingSigmasPerLevel( smoothingSigmasPerLevel );
  registration->SetShrinkFactorsPerLevel( shrinkFactorsPerLevel );
  
  //Do registration
  try{
	  registration->SetNumberOfThreads(1);
	  registration->Update();
  }
  catch( itk::ExceptionObject & err ){
	  std::cerr << "ExceptionObject caught !" << std::endl;
	  std::cerr << err << std::endl;
	  //return EXIT_FAILURE;
  }


  const double bestValue = optimizer->GetValue();
  std::cout << "Result = " << std::endl;
  std::cout << " Metric value  = " << bestValue          << std::endl;

  std::cout << "Registered transform parameters: " << std::endl;
  std::cout <<  registration->GetTransform()->GetParameters()  << std::endl;
  std::cout <<  transform->GetCenter()  << std::endl;


  SimilarityTransformType::Pointer inverse = SimilarityTransformType::New();
  transform->GetInverse( inverse );

  ////
  //7. Create registered overlay image
  ////
  ResampleFilterType::Pointer resampler = ResampleFilterType::New();
  resampler->SetInput( moving );
  resampler->SetTransform( inverse );
  resampler->SetSize( stemSize );
  resampler->SetOutputOrigin(  stemOrigin );
  resampler->SetOutputSpacing( stemSpacing );
  resampler->SetOutputDirection( stemImage->GetDirection() );
  resampler->SetDefaultPixelValue( 0 );
  resampler->Update();
  ImageType::Pointer moved = resampler->GetOutput();

  stem.aligned = moved;



#ifdef DEBUG
  ImageIO<ImageType>::WriteImage(moved, catStrings(prefix, "-stem-registered.tif") );

  moved = ITKFilterFunctions<ImageType>::ThresholdAbove(moved, 5, 255);

  CastFilter::Pointer movingCast = CastFilter::New();
  movingCast->SetInput( moved );

  BinaryImageToLabelMapFilterType::Pointer binaryImageToLabelMapFilter = BinaryImageToLabelMapFilterType::New();
  binaryImageToLabelMapFilter->SetInput( movingCast->GetOutput() );
  binaryImageToLabelMapFilter->Update();
 
  LabelMapToLabelImageFilterType::Pointer labelMapToLabelImageFilter = LabelMapToLabelImageFilterType::New();
  labelMapToLabelImageFilter->SetInput(binaryImageToLabelMapFilter->GetOutput());
  labelMapToLabelImageFilter->Update();
 
  ImageType::Pointer stemOrigRescaled = ITKFilterFunctions<ImageType>::Rescale( stemImageOrig, 0, 255);
  LabelOverlayImageFilterType::Pointer labelOverlayImageFilter = LabelOverlayImageFilterType::New();
  labelOverlayImageFilter->SetInput( stemOrigRescaled );
  labelOverlayImageFilter->SetLabelImage(labelMapToLabelImageFilter->GetOutput());
  labelOverlayImageFilter->SetOpacity(.25);
  labelOverlayImageFilter->Update();



  ImageIO<RGBImageType>::WriteImage( labelOverlayImageFilter->GetOutput(), catStrings(prefix, "-stem-overlay.png") );
#endif



  ////
  // 7. Transfrom initial estimates
  ////
  SimilarityTransformType::InputPointType tCenter;
  tCenter[0] = stem.initialCenter[0];
  tCenter[1] = stem.initialCenter[1];
  
  SimilarityTransformType::InputVectorType tX;
  tX[0] = stem.initialWidth;
  tX[1] = 0;

  stem.center = inverse->TransformPoint(tCenter);
  stemImage->TransformPhysicalPointToIndex(stem.center, stem.centerIndex);

  SimilarityTransformType::OutputVectorType tXO = inverse->TransformVector(tX, tCenter);

  stem.width =  sqrt(tXO[0]*tXO[0] + tXO[1]*tXO[1]); 


  std::cout << "Stem center: " << stem.centerIndex << std::endl;
  std::cout << "Stem width: "  << stem.width*2 << std::endl;


  std::cout << "--- Done fitting stem ---" << std::endl << std::endl;

  return stem;
};












int main(int argc, char **argv ){

  //Command line parsing
  TCLAP::CmdLine cmd("Fit stem to eye ultrasound", ' ', "1");

  TCLAP::ValueArg<std::string> imageArg("i","image","Ultrasound input image", true, "",
      "filename");
  cmd.add(imageArg);

  TCLAP::ValueArg<std::string> prefixArg("p","prefix","Prefix for storing output images", true, "",
      "filename");
  cmd.add(prefixArg);
  
  TCLAP::SwitchArg noiArg("","noimage","Do not output overlay image" );
  cmd.add(noiArg);

  try{
    cmd.parse( argc, argv );
  } 
  catch (TCLAP::ArgException &e){ 
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; 
    return -1;
  }

  std::string prefix = prefixArg.getValue();
  
  ////
  //1. Read and preprocess the ultrasound image
  ////
  ImageType::Pointer origImage = ImageIO<ImageType>::ReadImage( imageArg.getValue() );      

  /////
  //2. Fit eye
  ////
  Eye eye = fitEye( origImage, prefix );

  ////
  //3. Fit stem using eye size and location estimates
  ////
  Stem stem = fitStem( origImage, eye, prefix );
    
 
  if(noiArg.getValue() ){
	 return EXIT_SUCCESS;
  }


  ////
  //4. Create overlay image
  ////
  ImageType::Pointer moved = ImageIO<ImageType>::CopyImage( eye.aligned );
   
  itk::ImageRegionIterator<ImageType> eyeIterator(moved, stem.originalImageRegion );

  std::cout << stem.aligned->GetLargestPossibleRegion() << std::endl;
  itk::ImageRegionIterator<ImageType> stemIterator(stem.aligned, stem.aligned->GetLargestPossibleRegion() );
  while( !eyeIterator.IsAtEnd() ){
    
    eyeIterator.Set( eyeIterator.Get() + stemIterator.Get() );
    ++eyeIterator;
    ++stemIterator;
  }

  moved = ITKFilterFunctions<ImageType>::ThresholdAbove(moved, 5, 255);
  CastFilter::Pointer movingCast = CastFilter::New();
  movingCast->SetInput( moved );

#ifdef DEBUG
  ImageIO<ImageType>::WriteImage( moved, catStrings(prefix, "-joint-moved.tif") );
#endif
  
  BinaryImageToLabelMapFilterType::Pointer binaryImageToLabelMapFilter = BinaryImageToLabelMapFilterType::New();
  binaryImageToLabelMapFilter->SetInput( movingCast->GetOutput() );
  binaryImageToLabelMapFilter->Update();
 
  LabelMapToLabelImageFilterType::Pointer labelMapToLabelImageFilter = LabelMapToLabelImageFilterType::New();
  labelMapToLabelImageFilter->SetInput(binaryImageToLabelMapFilter->GetOutput());
  labelMapToLabelImageFilter->Update();
 
  ImageType::Pointer imageRescaled = ITKFilterFunctions<ImageType>::Rescale( origImage, 0, 255);
  LabelOverlayImageFilterType::Pointer labelOverlayImageFilter = LabelOverlayImageFilterType::New();
  labelOverlayImageFilter->SetInput( imageRescaled );
  labelOverlayImageFilter->SetLabelImage(labelMapToLabelImageFilter->GetOutput());
  labelOverlayImageFilter->SetOpacity(.25);
  labelOverlayImageFilter->Update();



  ImageIO<RGBImageType>::WriteImage( labelOverlayImageFilter->GetOutput(), catStrings(prefix, "-overlay.png") );
  return EXIT_SUCCESS;
}