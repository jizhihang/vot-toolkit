/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * This example is based on the camshiftdemo.c code from the OpenCV repository
 * It compiles with OpenCV 2.1
 * The main function of this example is to show the developers how to modify
 * their trackers to work with the evaluation environment.
 */

#ifdef _CH_
#pragma package <opencv>
#endif

#define CV_NO_BACKWARD_COMPATIBILITY

#ifndef _EiC
#include <cv.h>
#include <highgui.h>
#include <stdio.h>
#include <ctype.h>
#endif

#include "vot.h"

IplImage *image = 0, *hsv = 0, *hue = 0, *mask = 0, *backproject = 0, *histimg = 0;
CvHistogram *hist = 0;

int track_object = 0;
VOTPolygon selection;
CvRect track_window;
CvBox2D track_box;
CvConnectedComp track_comp;
int hdims = 16;
float hranges_arr[] = {0,180};
float* hranges = hranges_arr;
int vmin = 10, vmax = 256, smin = 30;

// Convert HSV to RGB
CvScalar hsv2rgb( float hue ) {
    int rgb[3], p, sector;
    static const int sector_data[][3]=
        {{0,2,1}, {1,2,0}, {1,0,2}, {2,0,1}, {2,1,0}, {0,1,2}};
    hue *= 0.033333333333333333333333333333333f;
    sector = cvFloor(hue);
    p = cvRound(255*(hue - sector));
    p ^= sector & 1 ? 255 : 0;

    rgb[sector_data[sector][0]] = 255;
    rgb[sector_data[sector][1]] = 0;
    rgb[sector_data[sector][2]] = p;

    return cvScalar(rgb[2], rgb[1], rgb[0],0);
}

// Convert rotated box to bounding box needed by the VOT toolkit
VOTPolygon convertbox(CvBox2D box) {

    double _angle = box.angle*CV_PI/180.0; 
    float a = (float)cos(_angle)*0.5f; 
    float b = (float)sin(_angle)*0.5f; 

    VOTPolygon p;
    p.x1 = box.center.x - a*box.size.height - b*box.size.width;
    p.y1 = box.center.y + b*box.size.height - a*box.size.width;
    p.x2 = box.center.x + a*box.size.height - b*box.size.width;
    p.y2 = box.center.y - b*box.size.height - a*box.size.width;
    p.x3 = 2 * box.center.x - p.x1;
    p.y3 = 2 * box.center.y - p.y1;
    p.x4 = 2 * box.center.x - p.x2;
    p.y4 = 2 * box.center.y - p.y2;

    return p;
}


int main( int argc, char** argv)
{

    int f = 0;

    // *****************************************
    // VOT: Call vot_initialize at the beginning
    // *****************************************

    //TODO: Convert selection?
    selection = vot_initialize();

    track_object = -1;

    for(f = 1;; f++)
    {
        int i, bin_w, c;

        // *****************************************
        // VOT: Call vot_frame to get path of the 
        //      current image frame. If the result is
        //      null, the sequence is over.
        // *****************************************
        const char* imagefile = vot_frame();
        if (!imagefile) break;

        // In trax mode images are read from the disk. The master program tells the
        // tracker where to get them.
        IplImage* frame = cvLoadImage(imagefile, CV_LOAD_IMAGE_COLOR);

        if( !frame )
            break;

        if( !image )
        {
            /* allocate all the buffers */
            image = cvCreateImage( cvGetSize(frame), 8, 3 );
            image->origin = frame->origin;
            hsv = cvCreateImage( cvGetSize(frame), 8, 3 );
            hue = cvCreateImage( cvGetSize(frame), 8, 1 );
            mask = cvCreateImage( cvGetSize(frame), 8, 1 );
            backproject = cvCreateImage( cvGetSize(frame), 8, 1 );
            hist = cvCreateHist( 1, &hdims, CV_HIST_ARRAY, &hranges, 1 );
            histimg = cvCreateImage( cvSize(320,200), 8, 3 );
            cvZero( histimg );
        }

        cvCopy( frame, image, 0 );
        cvCvtColor( image, hsv, CV_BGR2HSV );

        if( track_object )
        {
            int _vmin = vmin, _vmax = vmax;

            cvInRangeS( hsv, cvScalar(0,smin,MIN(_vmin,_vmax),0),
                        cvScalar(180,256,MAX(_vmin,_vmax),0), mask );
            cvSplit( hsv, hue, 0, 0, 0 );

            if( track_object < 0 )
            {
                float max_val = 0.f;
                CvRect roi;


                //Convert rotated rectangle to axis-aligned rectangle
                roi.x = cvFloor(MIN(MIN(MIN(selection.x1, selection.x2), selection.x3), selection.x4)); 
                roi.y = cvFloor(MIN(MIN(MIN(selection.y1, selection.y2), selection.y3), selection.y4));
                roi.width = cvCeil(MAX(MAX(MAX(selection.x1, selection.x2), selection.x3), selection.x4)) - roi.x - 1; 
                roi.height = cvCeil(MAX(MAX(MAX(selection.y1, selection.y2), selection.y3), selection.y4)) - roi.y - 1; 

                cvSetImageROI( hue, roi );
                cvSetImageROI( mask, roi );
                cvCalcHist( &hue, hist, 0, mask );
                cvGetMinMaxHistValue( hist, 0, &max_val, 0, 0 );
                cvConvertScale( hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0 );
                cvResetImageROI( hue );
                cvResetImageROI( mask );
                track_window = roi;
                track_object = 1;

                cvZero( histimg );
                bin_w = histimg->width / hdims;
                for( i = 0; i < hdims; i++ )
                {
                    int val = cvRound( cvGetReal1D(hist->bins,i)*histimg->height/255 );
                    CvScalar color = hsv2rgb(i*180.f/hdims);
                    cvRectangle( histimg, cvPoint(i*bin_w,histimg->height),
                                 cvPoint((i+1)*bin_w,histimg->height - val),
                                 color, -1, 8, 0 );
                }
            }

            cvCalcBackProject( &hue, backproject, hist );
            cvAnd( backproject, mask, backproject, 0 );
            if (track_window.width > 0 && track_window.height > 0) {
                cvCamShift( backproject, track_window,
                            cvTermCriteria( CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1 ),
                            &track_comp, &track_box );
                track_window = track_comp.rect;
            }

            if( !image->origin )
                track_box.angle = -track_box.angle;
        }

        // *****************************************
        // *****************************************
        vot_report(convertbox(track_box));

    }



    // *************************************
    // VOT: Call vot_deinitialize at the end
    // *************************************
    vot_deinitialize();

    return 0;
}

#ifdef _EiC
main(1,"camshiftdemo.c");
#endif
