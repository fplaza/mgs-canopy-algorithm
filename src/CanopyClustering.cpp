/**
 * Metagenomics Canopy Clustering Implementation
 *
 * Copyright (C) 2013, 2014 Piotr Dworzynski (piotr@cbs.dtu.dk), Technical University of Denmark
 * Copyright (C) 2015 Enterome
 *
 * This file is part of Metagenomics Canopy Clustering Implementation.
 *
 * Metagenomics Canopy Clustering Implementation is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Metagenomics Canopy Clustering Implementation is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <fstream>
#include <ctime>

#include <algorithm>

#include <omp.h>

#include <boost/foreach.hpp>


#include <CanopyClustering.hpp>
#include <Log.hpp>

#include <signal_handlers.hpp>
#include <prog_bar_misc.hpp>

Canopy* CanopyClusteringAlg::create_canopy(const Point* origin, const vector<Point*>& points, vector<Point*>& close_points, double max_neighbour_dist, double max_close_dist, bool set_close_points){

	std::vector<Point*> neighbours;

	if(set_close_points){
		Point* potential_neighbour; 

		//Go through all points and set the close points to contain the ones that are "close"
		close_points.clear();//Will not reallocate
		for(size_t i=0; i<points.size(); i++){

			potential_neighbour = points[i];

			double dist = get_distance_between_points(origin, potential_neighbour);

			if(dist < max_close_dist){

				close_points.push_back(potential_neighbour);

				if(dist < max_neighbour_dist){

					neighbours.push_back(potential_neighbour);

				}
			} 
		}

	} else {

		Point* potential_neighbour;

		for(size_t i=0; i<close_points.size(); i++){

			potential_neighbour = close_points[i];

			double dist = get_distance_between_points(origin, potential_neighbour);

			if(dist < max_neighbour_dist){
				neighbours.push_back(potential_neighbour);
			}
		}

	}
	
	if(neighbours.empty()){
        return new Canopy(origin);
    } else {
        return new Canopy(neighbours);
    }
}

Canopy* CanopyClusteringAlg::canopy_walk(const Point* origin, const vector<Point*>& points, vector<Point*>& close_points, double max_canopy_dist, double max_close_dist, double min_step_dist, double max_num_canopy_walks, int& num_canopy_jumps){

    Canopy *c1;
    Canopy *c2;


    c1 = create_canopy(origin, points, close_points, max_canopy_dist, max_close_dist, true);
    c2 = create_canopy(c1->center, points, close_points, max_canopy_dist, max_close_dist, false);
    
    double dist = get_distance_between_points(c1->center, c2->center);

    {
        _log(logDEBUG2) << "Canopy walking, first step";
        _log(logDEBUG2) << *c1;
        _log(logDEBUG2) << *c2;
        _log(logDEBUG2) << "First potential jump correlation dist: " << dist;
    }

    int num_canopy_jumps_local = 0;
    while((dist > min_step_dist) && (num_canopy_jumps_local <= max_num_canopy_walks )){
        delete c1;
        c1=c2;

        num_canopy_jumps_local++; 

#pragma omp atomic
        num_canopy_jumps++;

        c2=create_canopy(c1->center, points, close_points, max_canopy_dist, max_close_dist, false);
        dist = get_distance_between_points(c1->center, c2->center); 
        {
            _log(logDEBUG2) << "Canopy walking, step: " << num_canopy_jumps_local;
            _log(logDEBUG2) << *c1;
            _log(logDEBUG2) << *c2;
            _log(logDEBUG2) << "distance: " << dist;
        }
    }

    //Now we know that c1 and c2 are close enough and we should choose the one that has more neighbours
    Canopy* final_canopy; 
    if(c1->neighbours.size() > c2->neighbours.size()){
        final_canopy = c1;
        delete c2;
    } else {
        final_canopy = c2;
        delete c1;
    }
    return final_canopy;
}

void CanopyClusteringAlg::filter_clusters_by_size(std::vector<Canopy*>& canopies_to_filter){
	size_t canopies_kept = 0;
	for (size_t curr_canopy = 0; curr_canopy < canopies_to_filter.size(); curr_canopy ++) {
		Canopy* canopy = canopies_to_filter[curr_canopy];

        if(canopy->neighbours.size() < 2){
			delete canopy;
		} else {
			canopies_to_filter[canopies_kept++] =  canopy;
		}

	}
	canopies_to_filter.resize(canopies_kept);
}

void CanopyClusteringAlg::filter_clusters_by_single_point_skew(double max_single_data_point_proportion, std::vector<Canopy*>& canopies_to_filter){
	size_t canopies_kept = 0;
	for (size_t curr_canopy = 0; curr_canopy < canopies_to_filter.size(); curr_canopy ++) {
		Canopy* canopy = canopies_to_filter[curr_canopy];
		Point* ccenter = canopy->center;

        if(!ccenter->check_if_single_point_proportion_is_smaller_than(max_single_data_point_proportion) ){
			delete canopy;
		} else {
			canopies_to_filter[canopies_kept++] =  canopy;
		}

	}
	canopies_to_filter.resize(canopies_kept);
}

void CanopyClusteringAlg::filter_clusters_by_zero_medians(int min_num_non_zero_medians, std::vector<Canopy*>& canopies_to_filter){

	size_t canopies_kept = 0;
	for (size_t curr_canopy = 0; curr_canopy < canopies_to_filter.size(); curr_canopy ++) {
		Canopy* canopy = canopies_to_filter[curr_canopy];
		Point* ccenter = canopy->center;

		if(! ccenter->check_if_num_non_zero_samples_is_greater_than_x(min_num_non_zero_medians) ){
			delete canopy;
		} else {
			canopies_to_filter[canopies_kept++] =  canopy;
		}

	}
	canopies_to_filter.resize(canopies_kept);
}

std::vector<Canopy*> CanopyClusteringAlg::multi_core_run_clustering_on(vector<Point*>& points, double max_canopy_dist, double max_close_dist, double max_merge_dist, double min_step_dist, int max_num_canopy_walks, double stop_proportion_of_points, string canopy_size_stats_fp, string not_processed_points_fp, bool show_progress_bar, TimeProfile& time_profile){
    _log(logPROGRESS) << "";
    _log(logPROGRESS) << "############ Shuffling ############";
    time_profile.start_timer("Shuffling");
    std::srand ( unsigned ( std::time(NULL) ) );
    std::random_shuffle(points.begin(), points.end());
    time_profile.stop_timer("Shuffling");

    _log(logPROGRESS) << "";
    _log(logPROGRESS) << "############ Creating Canopies ############";
    boost::unordered_set<Point*> marked_points;//Points that should not be investigated as origins
	marked_points.rehash(points.size());

    vector<unsigned int> canopy_size_per_origin_num;//Contains size of the canopy created from origin by it's number, so first origin gave canopy of size 5, second origin gave canopy of size 8 and so on
    int last_progress_displayed_at_num_points = 0;

    std::vector<Canopy*> canopy_vector;

    vector<Point*> close_points;
    close_points.reserve(points.size());

    //
    //Create canopies
    //
    time_profile.start_timer("Clustering");
        
    ofstream canopy_size_stats_file;

    if(canopy_size_stats_fp != "")
        canopy_size_stats_file.open(canopy_size_stats_fp.c_str(), ios::out | ios::trunc);

    int canopy_stats_row_num = 0;

    int num_canopy_jumps = 0;
    int num_collisions = 0;

    size_t first_non_processed_origin_due_interruption = points.size();

#pragma omp parallel for shared(marked_points, canopy_vector, num_canopy_jumps, canopy_size_per_origin_num, num_collisions, canopy_stats_row_num, terminate_called, first_non_processed_origin_due_interruption) firstprivate(close_points, max_canopy_dist, max_close_dist, max_merge_dist, min_step_dist, last_progress_displayed_at_num_points) schedule(dynamic)
    for(size_t origin_i = 0; origin_i < points.size(); origin_i++){

        //Early stopping proportion of points
        if(marked_points.size() > stop_proportion_of_points * points.size()){
            continue;
        }

        //Stop if exit signal received
        if(terminate_called){
            if(first_non_processed_origin_due_interruption > origin_i){
#pragma omp critical
                {
                    first_non_processed_origin_due_interruption = origin_i;
                }
            }
            continue;
        }

        Point* origin = points[origin_i]; 

        if(marked_points.find(origin) != marked_points.end())
            continue;

        //Show progress bar
        {
            //Only master thread executes this
            if(omp_get_thread_num() == 0){
                if(Logger::log_level >= logPROGRESS && show_progress_bar){
                    if(marked_points.size() > last_progress_displayed_at_num_points + stop_proportion_of_points * points.size()/100){
                        printProgBar(marked_points.size(),stop_proportion_of_points * points.size());
                        last_progress_displayed_at_num_points = marked_points.size();
                    }
                }
            }
        }




        {
            _log(logDEBUG) << "Unmarked points count: " << points.size() - marked_points.size() << " Marked points count: " << marked_points.size();
            _log(logDEBUG) << "points.size: " << points.size() << " origin_i: " << origin_i << " origin->id: " << origin->id ;

            _log(logDEBUG1) << "Current canopy origin: " << origin->id;
        }

        Canopy* final_canopy = canopy_walk(origin, points, close_points, max_canopy_dist, max_close_dist, min_step_dist, max_num_canopy_walks, num_canopy_jumps);

#pragma omp critical
        {
            //Do not commit anything if by chance another thread marked the current origin
            if(marked_points.find(origin) == marked_points.end()){

                //Add canopy
                marked_points.insert(origin);

                canopy_vector.push_back(final_canopy);

                BOOST_FOREACH(Point* n, final_canopy->neighbours){
                    marked_points.insert(n);
                }

                //Statistics showing size of canopies per analyzed origin
                if(canopy_size_stats_file.is_open()){
                    canopy_size_stats_file << canopy_stats_row_num++  << "\t" << points.size() - marked_points.size() << "\t" << final_canopy->neighbours.size() << "\t" << num_collisions << endl;
                }

            } else {
                num_collisions++;
                delete final_canopy;
            }

        }

    }
    if(canopy_size_stats_file.is_open())
        canopy_size_stats_file.close();

    time_profile.stop_timer("Clustering");
    
    if(terminate_called && (not_processed_points_fp != "")){
        time_profile.start_timer("Saving unprocessed points file");
        _log(logERR) << "Received signal, clustering was stopped early, saving non processed points in file: " << not_processed_points_fp; 
        cout << "first_non_processed_origin_due_interruption:" << first_non_processed_origin_due_interruption << endl; 

        ofstream not_processed_points_file;
        not_processed_points_file.open(not_processed_points_fp.c_str(), ios::out | ios::trunc);
        for(size_t i = first_non_processed_origin_due_interruption; i < points.size(); i++){
            Point* point = points[i];
            if(marked_points.find(point) == marked_points.end()){
                not_processed_points_file << point->id;
                for(size_t j = 0; j < point->num_data_samples; j++){
                    not_processed_points_file << "\t" << point->sample_data[j];
                }
                not_processed_points_file << "\n";
            }
        }
        not_processed_points_file.close();
        _log(logERR) << "Unprocessed points saved"; 
        time_profile.stop_timer("Saving unprocessed points file");
    }


    _log(logINFO) << "";
    _log(logINFO) << "Avg. number of canopy walks: " << num_canopy_jumps/((double)canopy_vector.size());
    _log(logINFO) << "Number of canopies before merging: " << canopy_vector.size();

    int original_number_of_canopies = canopy_vector.size();

    //
    // Merge Canopies
    //
    std::vector<Canopy*> merged_canopy_vector;

    time_profile.start_timer("Merging");
    _log(logPROGRESS) << "";
    _log(logPROGRESS) << "############Merging canopies#############";

    while(canopy_vector.size()){

        std::vector<Canopy*> canopies_to_merge;

        //This is the canopy we will look for partners for
        Canopy* c = *canopy_vector.rbegin();
        canopy_vector.pop_back();

        canopies_to_merge.push_back(c);

        //Get canopies that are nearby
		size_t canopies_not_to_merge = 0;
        for(size_t i = 0; i < canopy_vector.size(); i++){

            Canopy* c2 = canopy_vector[i]; 

            double dist = get_distance_between_points(c->center, c2->center);

            if(dist < max_merge_dist){
				canopies_to_merge.push_back(c2);
            } else {
				canopy_vector[canopies_not_to_merge++] = c2;
			}
        }
		canopy_vector.resize(canopies_not_to_merge);

        //There are several canopies to merge, do it
        if( canopies_to_merge.size() > 1 ){

			boost::unordered_set<Point*> all_points_from_merged_canopies_tmp;
			all_points_from_merged_canopies_tmp.rehash(canopies_to_merge[0]->neighbours.size());
            
            BOOST_FOREACH(Canopy* canopy, canopies_to_merge)
                BOOST_FOREACH(Point* n, canopy->neighbours)
					all_points_from_merged_canopies_tmp.insert(n);

			std::vector<Point*> all_points_from_merged_canopies(all_points_from_merged_canopies_tmp.begin(), all_points_from_merged_canopies_tmp.end());

            //Create new canopy
            Point* temp_merged_canopy_origin = get_centroid_of_points(all_points_from_merged_canopies);
            Canopy* merged_canopy = canopy_walk(temp_merged_canopy_origin, all_points_from_merged_canopies, close_points, max_canopy_dist, max_close_dist, min_step_dist, max_num_canopy_walks, num_canopy_jumps);
            delete temp_merged_canopy_origin;


            canopy_vector.push_back(merged_canopy);

            
            //Removed merged canopies 
            BOOST_FOREACH(Canopy* canopy, canopies_to_merge){
                delete canopy;
            }


        //If no canopies were merged remove the canopy we compared against the others
        } else {

            merged_canopy_vector.push_back(c);

            //Show progress bar
            {
                if(Logger::log_level >= logPROGRESS && show_progress_bar){
                    if(original_number_of_canopies - canopy_vector.size() % 1000)
                        printProgBar(original_number_of_canopies - canopy_vector.size(), original_number_of_canopies );
                }
            }
        }
    }
    time_profile.stop_timer("Merging");

    _log(logINFO) << "";
    _log(logINFO) << "Number of canopies after merging: " << merged_canopy_vector.size();

    return merged_canopy_vector;

}

