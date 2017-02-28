#include <minc2.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <vector>
#include <math.h>
#include <map>
#include <set>
#include <ParseArgv.h>
#include <queue>

#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmimgle/dcmimage.h"
#include "dcmtk/dcmdata/dcpxitem.h"

using namespace std;

/* Set variables for handling the input volumes */
mihandle_t	container_volume, label_volume;
midimhandle_t dimensions[3], *dimensions_new;
char 		*pname;
char 		**infiles, **outfiles;

/* Varies for the RT struct and MINC volume */
int 		i,j,slice, iter, coordinate_counter, first_voxel_at_slice, xyz_counter, cord_i, vox_i, contour_voxel, TargetColour, NewColour, starting_point, WestNode, EastNode, SouthNode, NorthNode, Node;
misize_t start[3], counter[3];
unsigned int  sizes[3];
double        *slab, *tmpSlice;
double        world_location[3];
double        dvoxel_location[3];
unsigned long voxel_location[3];
map< int,vector< vector<double> > > contours;


/*Main start */
int main(int argc, char **argv)
{

	/* Input handling */
	pname = argv[0];
	if(argc < 2)
	{
      	(void) fprintf(stderr, "\nUsage: %s <VOLUME.mnc> <RTx> <out_label.mnc>\n", pname);
      	(void) fprintf(stderr, "       <VOLUME.mnc>\tis the file which the RTx was defined on.\n");
      	(void) fprintf(stderr, "                   \tThe out_label will have the dimensions and voxel size matching this file.\n");
      	(void) fprintf(stderr, "       <RTx>\t\tis the RT struct in DICOM format.\n");
      	(void) fprintf(stderr, "       <out_label.mnc>\tis the resulting MINC file with the contours in the RT file set to 1.\n\n");
		exit(EXIT_FAILURE);
	}
	outfiles = &argv[argc-1];
	infiles = &argv[1];

	/* READ DICOM RT STRUCT
	 * The file is formatted as (one) ROIContourSequence
	 *						 -> (Abitrary number of) ContourSequence
	 *						 -> -> ContourNumber, NumberOfContourPoints and ContourData
	 * Contour data is a double-string consisting of x\y\z\[..]\x\y\z coordinates.
	 *
	*/

	fprintf(stderr, "Loading RTx file: %s\n",infiles[1]);


	DcmFileFormat fileformat;
	//http://support.dcmtk.org/redmine/projects/dcmtk/wiki/Howto_GetSequenceItem
  	if (fileformat.loadFile(infiles[1]).good()){
  		// Load the DICOM-RT dataset
  		DcmDataset *dataset = fileformat.getDataset();
  		// Find the ROIContourSequence tag
  		DcmSequenceOfItems *pROIContourSequnce = NULL;
  		if(dataset->findAndGetSequence(DCM_ROIContourSequence, pROIContourSequnce, true).good()){
  			// Assuming only 1 - get it.
  			DcmDataset *ROIContourSequence = (DcmDataset *) pROIContourSequnce->getItem(0);
  			// Find the ContourSequence tags
  			DcmSequenceOfItems *pContourSequnce = NULL;
  			if(ROIContourSequence->findAndGetSequence(DCM_ContourSequence, pContourSequnce, true).good()){
  				// Get the number of contour sequences in the file
  				Uint16 numimages = pContourSequnce->card();
					//Print number of contour sequences in the file
  				fprintf(stderr, "\tFound %d contour data entries in the RTx file.\n",numimages);
					//For each contour data entry do the following:
  				for( iter=0; iter<numimages; iter++){

  					// Get each ContourSequence at a time
  					DcmItem *ContourSequence = pContourSequnce->getItem(iter);

  					// For future reference, get the contour Number and number of points in sequence

						//Define char pointer
  					const char *number = NULL;
						//Tag DCM_ContourNumber(3006, 0048)
  					ContourSequence->findAndGetString(DCM_ContourNumber, number, 4);
						//Turn into string
  					string s_number(number);
						//Define char pointer
  					const char *npoints = NULL;
						//Tag DCM_NumberOfContourPoints(3006, 0046)
  					ContourSequence->findAndGetString(DCM_NumberOfContourPoints, npoints, 4);
						//Turn into string
  					string s_npoints(npoints);
						//	printf("number of contour points %s\n", npoints);
  					// Get the contour data points in the form
  					vector< vector<double> > xyz_coordinates;
						//Define char pointer
						const char *cdata = NULL;
						//Tag DCM_ContourData (3006,0050)
  					ContourSequence->findAndGetString(DCM_ContourData, cdata, 4);
  					string s_cdata(cdata);
						//printf("%s\n", cdata);
  					// Extract each combination of x,y,z coordinates and insert them in map
  					size_t pos = 0;
  					string token;
  					string delimiter = "\\";
  					coordinate_counter = 0;
						xyz_counter = 0;

  					while((pos = s_cdata.find(delimiter)) != string::npos){
							//http://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
  						token = s_cdata.substr(0,pos);

  						// If 0, we are at x again, add new coordinate point to vector
  						if(xyz_counter == 0){
  							vector<double> new_point(1,3);
  							xyz_coordinates.push_back(new_point);
  						}

  						// Add coordinate point (x, y or z) to vector
  						xyz_coordinates[coordinate_counter].push_back(atof(token.c_str()));

  						// If 3, we have added z, reset to x for next iteration
  						if(xyz_counter+1 == 3){
  							xyz_counter = 0;
  							coordinate_counter += 1;
  						} else {
  							xyz_counter += 1;
  						}

  						s_cdata.erase(0,pos+delimiter.length());
  					}
  					// Add final z that is left in the sequence
  					xyz_coordinates[coordinate_counter].push_back(atof(s_cdata.c_str()));



  					// Add all points from contour to map of contours
  					contours.insert( pair< int,vector< vector<double> > >(iter,xyz_coordinates));


						// while((pos = s_cdata.find(delimiter)) != string::npos){
						// 	token = s_cdata.substr(0,pos);
						// 	vector<double> new_point(1,3);
						// 	xyz_coordinates.push_back(new_point);
						// 	xyz_counter = 0;
						// 	cout << "Hello2";
						// 	while(xyz_counter != 3){
						// 		xyz_coordinates[coordinate_counter].push_back(atof(token.c_str()));
						// 		xyz_counter += 1;
						// 		coordinate_counter += 1;
						// 		s_cdata.erase(0,pos+delimiter.length());
						// 	}
						// }
						// xyz_coordinates[coordinate_counter].push_back(atof(s_cdata.c_str()));
						// contours.insert( pair< int,vector< vector<double> > >(iter,xyz_coordinates));
  				}

  			}

  		}

  	}

	/* Load the MINC container file from which we will obtain
	 * information such as voxel size and image dimensions.
	 *
	 * We can only work with minc 2.0 files */

  fprintf(stderr, "Loading PET file: %s\n",infiles[0]);

	system("mkdir /tmp/minc_plugins/");
	std::string in1 = "mincconvert -clobber ";
	in1 += infiles[0];
	in1 += " -2 /tmp/minc_plugins/in.mnc";
	system(in1.c_str());


	if (miopen_volume("/tmp/minc_plugins/in.mnc", MI2_OPEN_RDWR, &container_volume) != MI_NOERROR)
	{
		fprintf(stderr, "Error opening input file: %s.\n",infiles[0]);
		exit(EXIT_FAILURE);
	}
 	/* Taken from https://en.wikibooks.org/wiki/MINC/Tutorials/Programming05*/
	// get the dimension sizes
	miget_volume_dimensions(container_volume, MI_DIMCLASS_SPATIAL, MI_DIMATTR_ALL, MI_DIMORDER_FILE, 3, dimensions);
	misize_t sizes_tmp[3];
	miget_dimension_sizes(dimensions, 3, sizes_tmp);
	// allocate new dimensions
	dimensions_new = (midimhandle_t *) malloc(sizeof(midimhandle_t) * 3);
	//std::vector<midimhandle_t> dimensions_new(sizeof(midimhandle_t) * 3);
	for (i=0; i < 3; i++) {
		//////////////////Why is this here///////////////////////
		sizes[i] = (unsigned int) sizes_tmp[i];
		counter[i] = (unsigned long) sizes[i];
		micopy_dimension(dimensions[i], &dimensions_new[i]);
	}


	fprintf(stderr, "\tDimensions of PET file: %dx%dx%d\n",sizes[0],sizes[1],sizes[2]);

	/* create the new volume */
	if (micreate_volume("/tmp/minc_plugins/label_volume.mnc", 3, dimensions_new, MI_TYPE_UBYTE,
	MI_CLASS_REAL, NULL, &label_volume) != MI_NOERROR) {
		fprintf(stderr, "Error creating new volume\n");
		return(1);
	}
	/* indicate that we will be using slice scaling */
  miset_slice_scaling_flag(label_volume, TRUE);

  /* set valid range */
	miset_volume_valid_range(label_volume, 255, 0);

	/* create the data for the new volume*/
	if (micreate_volume_image(label_volume) != MI_NOERROR) {
		fprintf(stderr, "Error creating volume data\n");
		return(1);
	}


	/* the start and counts */
	start[0] = start[1] = start[2] = 0;
	// First allocate enough memory to hold one slice of data
	//std::vector<double> slab(sizes[0]*sizes[1]*sizes[2]);
	slab = (double*) malloc(sizeof(double) * sizes[0] * sizes[1] * sizes[2]);

	// Initialize new hyperslab for the label file with zeros everywhere
  for (i=0; i < sizes[0] * sizes[1] * sizes[2]; i++) {
		slab[i] = 0.0;
	}


	/* Loop through the RT struct map, set new label to 1 for every pixel on the contours */
	map< int , vector < vector<double> > >::iterator it = contours.begin();
  	for(it=contours.begin(); it!= contours.end(); ++it){
  		vector< vector<double> > contour = it->second;

			// create 400x400 matrix
			std::vector<double> tmpSlice(sizes[1]*sizes[2]);
			//tmpSlice = new double[ sizes[1]*sizes[2] ];

			//Fill 400x400 matrix with 0's
			for (i=0; i < sizes[1] * sizes[2]; i++){
				tmpSlice[i] = 0.0;
			}

		for(cord_i = 0; cord_i < contour.size(); ++cord_i){
			// Convert to voxel coordinate
			world_location[0] = -contour[cord_i][1]; // manual -1* (why?)
			world_location[1] = -contour[cord_i][2]; // manual -1* (why?)
			world_location[2] = contour[cord_i][3];
			miconvert_world_to_voxel(container_volume, world_location, dvoxel_location);
			for (vox_i=0; vox_i<3; vox_i++){
								voxel_location[vox_i] = (unsigned long) dvoxel_location[vox_i];
			}

			// Calculate the position in the hyperslab
			//int contour_voxel = voxel_location[0]*sizes[1]*sizes[2] + voxel_location[1]*sizes[2] + voxel_location[2];
			//if(slice == 0){
			first_voxel_at_slice = voxel_location[0]*sizes[1]*sizes[2];
			//}

			contour_voxel = voxel_location[1]*sizes[2] + voxel_location[2];
			//slab[contour_voxel] = 1.0;
			// Set new label value to 1
			tmpSlice[contour_voxel] = 1.0;

		}
		/* Flood fill */
		//Doing this contour number of times.
		//Colour we are looking for as background
		TargetColour = 0.0;
		//The new colour we will write
		NewColour = 2.0;
		//create set
		set < int > Set;
		//Create queue
		queue < int > MyQue;
		int coun = 0;
		//Insert first point into the queue
		MyQue.push(first_voxel_at_slice);
		//While loop for iterating over the nodes.
		while (!MyQue.empty()){
			//Set front element to Node, and pop the front element from queue
			Node = MyQue.front();
			MyQue.pop();

			coun = coun + 1;
			//Change the colour to newcolour
			tmpSlice[Node] = NewColour;

			//Define the Node directions
			WestNode = Node-1;
			EastNode = Node+1;
			//sizes are the lengths x,y
			NorthNode = Node-sizes[1];
			SouthNode = Node+sizes[2];
			std::cout << "MyQue size: " << MyQue.size() << '\n';

			//East Node
			if (tmpSlice[EastNode] == TargetColour && floor((Node-sizes[1]*sizes[2]*floor(Node/(sizes[1]*sizes[2])))/sizes[1]) == floor((EastNode-sizes[1]*sizes[2]*floor(EastNode/(sizes[1]*sizes[2])))/sizes[1])){
				if (Set.insert(EastNode).second) {
    				MyQue.push(EastNode);
					}
			}

			//West Node
			if (tmpSlice[WestNode] == TargetColour && floor((Node-sizes[1]*sizes[2]*floor(Node/(sizes[1]*sizes[2])))/sizes[1]) == floor((WestNode-sizes[1]*sizes[2]*floor(WestNode/(sizes[1]*sizes[2])))/sizes[1])){
				if (Set.insert(WestNode).second) {
    				MyQue.push(WestNode);
					}
			}

			//North Node
			if (tmpSlice[NorthNode] == TargetColour && floor(Node / (sizes[1]*sizes[2])) == floor(NorthNode / (sizes[1]*sizes[2]))){
				if (Set.insert(NorthNode).second) {
    				MyQue.push(NorthNode);
					}
			}

			//South Node
			if (tmpSlice[SouthNode] == TargetColour && floor(Node / (sizes[1]*sizes[2])) == floor(SouthNode / (sizes[1]*sizes[2]))){
				if (Set.insert(SouthNode).second) {
    				MyQue.push(SouthNode);
					}
			}

		}
		// Insert inner region into slab runs sizes[1] * sizes[2] = 160000 times
		for(i = 0; i < sizes[1]*sizes[2]; i++){
			if(tmpSlice[i] < NewColour){
				slab[first_voxel_at_slice + i] += 1.0;
			}

		}
		fprintf(stderr, "Flood while: %i \n", coun);
	}
	int cnt = 0;
	std::cout << "Outside of scope: " << cnt << '\n';

	for (i=0; i < sizes[0] * sizes[1] * sizes[2]; i++) {
	 slab[i] = ((fmod(slab[i], 2.0) == 0.0) ? 0.0 : 1.0);
	}


	fprintf(stderr, "Writing volume and data to file: %s\n",outfiles[0]);
	////////Code below is taken from //////////////Taken from https://en.wikibooks.org/wiki/MINC/Tutorials/Programming05 //////



	/* set the slice scaling */
	miset_slice_range(label_volume, start, 3, 1, 0);

	// put the hyperslab into the new volume
	if (miset_real_value_hyperslab(label_volume, MI_TYPE_DOUBLE,
		start, counter, slab) != MI_NOERROR) {
		fprintf(stderr, "Error setting new hyperslab volume\n");
		return(1);
	}

	// closes the volume and makes sure all data is written to file
	miclose_volume(container_volume);
	miclose_volume(label_volume);

	// free memory
	//free(dimensions_new);
	//free(tmpSlice);
	//free(slab);

	// Convert back to minc 1 volume and save to wanted location
	std::string s_outfiles = "mincconvert -clobber /tmp/minc_plugins/label_volume.mnc ";
	s_outfiles += outfiles[0];
	system(s_outfiles.c_str());

	system("rm -rf /tmp/minc_plugins/");

	fprintf(stderr, "Finished rtx2mnc\n");

	return(0);
}
