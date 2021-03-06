#ifndef ITKFILTERFUNCTIONS_H
#define ITKFILTERFUNCTIONS_H


#include "itkImage.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkThresholdImageFilter.h"
#include "itkSmoothingRecursiveGaussianImageFilter.h"
#include "itkSubtractImageFilter.h"
#include "itkAddImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"

template < typename TImage >
class ITKFilterFunctions{


  public:

    typedef TImage Image;
    typedef typename Image::Pointer ImagePointer;
    typedef typename Image::ConstPointer ImageConstPointer;
    typedef typename Image::IndexType ImageIndex;
    typedef typename Image::PixelType PixelType;
    typedef typename Image::RegionType ImageRegion;
    typedef typename ImageRegion::SizeType ImageSize;
    typedef typename ImageSize::SizeValueType ImageSizeValue;
    typedef typename Image::SpacingType ImageSpacing;
    typedef typename Image::PixelType Precision;


    typedef typename itk::RescaleIntensityImageFilter<Image, Image> RescaleFilter;
    typedef typename RescaleFilter::Pointer RescaleFilterPointer;

    typedef typename itk::SmoothingRecursiveGaussianImageFilter<Image, Image> GaussianFilter;
    typedef typename GaussianFilter::Pointer GaussianFilterPointer;
    typedef typename GaussianFilter::SigmaArrayType SigmaArrayType;
    
    typedef typename itk::ThresholdImageFilter<Image>  ThresholdFilter;
    typedef typename ThresholdFilter::Pointer ThresholdFilterPointer;

    typedef typename itk::SubtractImageFilter<Image, Image> SubtractFilter;
    typedef typename SubtractFilter::Pointer SubtractFilterPointer;
    
    typedef typename itk::AddImageFilter<Image, Image> AddFilter;
    typedef typename AddFilter::Pointer AddFilterPointer;
  
    typedef itk::BinaryThresholdImageFilter <Image, Image> BinaryThresholdFilter;
    typedef typename BinaryThresholdFilter::Pointer BinaryThresholdFilterPointer;

  static ImagePointer Rescale(ImagePointer image, PixelType minI, PixelType maxI){
    RescaleFilterPointer rescale = RescaleFilter::New();
    rescale->SetInput(image);
    rescale->SetOutputMaximum( maxI );
    rescale->SetOutputMinimum( minI );
    rescale->Update();

    return rescale->GetOutput();
    
  };
 

  static ImagePointer GaussSmooth(ImagePointer image, SigmaArrayType sigma){
    GaussianFilterPointer smooth = GaussianFilter::New();
    smooth->SetSigmaArray( sigma );
    smooth->SetInput( image );
    smooth->Update();
    return smooth->GetOutput();
  };


  static ImagePointer ThresholdAbove(ImagePointer image, PixelType t, PixelType outside){
    ThresholdFilterPointer thresholdFilter  = ThresholdFilter::New();
    thresholdFilter->SetInput( image);
    thresholdFilter->ThresholdAbove( t );
    thresholdFilter->SetOutsideValue( outside );
    thresholdFilter->Update();  
    return thresholdFilter->GetOutput(); 
  };
 
 
  static ImagePointer ThresholdBelow(ImagePointer image, PixelType t, PixelType outside){
    ThresholdFilterPointer thresholdFilter  = ThresholdFilter::New();
    thresholdFilter->SetInput( image);
    thresholdFilter->ThresholdBelow( t );
    thresholdFilter->SetOutsideValue( outside );
    thresholdFilter->Update();  
    return thresholdFilter->GetOutput(); 
  };
   
  static ImagePointer BinaryThreshold(ImagePointer image, PixelType tLow, PixelType tHigh, PixelType inside, PixelType outside){
    BinaryThresholdFilterPointer thresholdFilter  = BinaryThresholdFilter::New();
    thresholdFilter->SetInput( image);
    thresholdFilter->SetLowerThreshold( tLow );
    thresholdFilter->SetUpperThreshold( tHigh );
    thresholdFilter->SetOutsideValue( outside );
    thresholdFilter->SetInsideValue( inside );
    thresholdFilter->Update();  
    return thresholdFilter->GetOutput(); 
  };

  static ImagePointer Subtract(ImagePointer i1, ImagePointer i2){
    SubtractFilterPointer subtract = SubtractFilter::New();
    subtract->SetInput1(i1);
    subtract->SetInput2(i2);
    subtract->Update();
    return subtract->GetOutput();
  };
  

  static ImagePointer Add(ImagePointer i1, ImagePointer i2){
    AddFilterPointer add = AddFilter::New();
    add->SetInput1(i1);
    add->SetInput2(i2);
    add->Update();
    return add->GetOutput();
  };
  







  
  static void AddBorder(ImagePointer image, int w){
    ImageSize size = image->GetLargestPossibleRegion().GetSize();
    AddHorizontalBorder(image, w);
    AddVerticalBorder(image, w);
  };
    
  
  static void AddHorizontalBorder(ImagePointer image, int w){

    ImageSize size = image->GetLargestPossibleRegion().GetSize();

    for(int i=0; i<size[0]; i++){
      ImageIndex index;
      index[0] = i;
      for(int j=0; j<w; j++){
	      index[1] = j;
	      image->SetPixel(index, 100);
	      index[1] = size[1]-1-j;
	      image->SetPixel(index, 100);
      }
    }    
  };

  
  static void AddVerticalBorder(ImagePointer image, int w){
    ImageSize size = image->GetLargestPossibleRegion().GetSize();
    for(int i=0; i<size[1]; i++){
      ImageIndex index;
      index[1] = i;
      for(int j=0; j<w; j++){
        index[0] = j;
        image->SetPixel(index, 100);
        index[0] = size[0]-1-j;
        image->SetPixel(index, 100);
      }
    }
  };




  static void RescaleRows(ImagePointer image){  

	  ImageSize size = image->GetLargestPossibleRegion().GetSize();

	  //Rescale row by row
	  for(int i=0; i<size[1]; i++){
		  ImageIndex index;
		  index[1] = i;
		  float maxIntensity = 0;
		  for(int j=0; j<size[0]; j++){
			  index[0] = j;
			  maxIntensity = std::max( image->GetPixel(index), maxIntensity );
		  }
		  for(int j=0; j<size[0]; j++){
			  index[0] = j;
			  image->SetPixel(index, image->GetPixel(index) / maxIntensity);
		  }
	  }
  };



};


#endif
