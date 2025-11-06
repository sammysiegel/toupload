#include "src/server_config.hh"
#include "src/background_server_batl.hh"
#include <iostream>
#include <iomanip>
#include <fstream>

using namespace Spectrum;

inline GeoVector drift_numer(double r_L, double vel, SpatialData spdata, int specie)
{
   GeoVector drift = (r_L * vel / 3.0) * (spdata.curlB() - 2.0 * (spdata.gradBmag ^ spdata.bhat)) / spdata.Bmag;
   // Correct magnitude if necessary
   if (drift.Norm() > 0.5 * vel)
   {
      drift.Normalize();
      drift *= 0.5 * vel;
   };
   return drift;
};

int main(int argc, char **argv)
{
   int active_local_workers, workers_stopped;
   BackgroundServerBATL background;

   SpatialData spdata;
   double t = 0.0;
   int i, j, k;
   GeoVector pos = gv_zeros, vel = gv_zeros, mom = gv_zeros;
   int specie = SPECIES_PROTON_BEAM;
   std::ofstream Den_file;
   std::ofstream AbsVel_file;
   std::ofstream AbsMag_file;
   std::ofstream PolMag_file;
   std::ofstream dmax_file;
   std::ofstream drift_file;
   std::ofstream HetFlx_file;
   std::ofstream TurEnr_file;
   std::string cir_date;

   std::ofstream data_file;

   //    if (argc > 1) {
   //       cir_date = argv[1];
   //       std::cout << "CIR date: " << cir_date << std::endl;
   //    } else {
   //       std::cout << "ERROR: No CIR date provided." << std::endl;
   //       exit(1);
   //    };

   //--------------------------------------------------------------------------------------------------
   // Server
   //--------------------------------------------------------------------------------------------------

   std::shared_ptr<MPI_Config> mpi_config = std::make_shared<MPI_Config>(argc, argv);
   std::shared_ptr<ServerBaseBack> server_back = nullptr;

   std::string fname_pattern = "/data001/multiscale_yh/project1/TurMHD/3d__var_3_t30000330_n00456411";

   if (mpi_config->is_boss)
   {
      server_back = std::make_unique<ServerBackType>(fname_pattern);
      active_local_workers = mpi_config->workers_in_node;
      server_back->ServerStart();
   };

   DataContainer container;

   //--------------------------------------------------------------------------------------------------
   // Background
   //--------------------------------------------------------------------------------------------------

   container.Clear();

   // Initial time
   double t0 = 0.0;
   container.Insert(t0);

   // Origin
   container.Insert(gv_zeros);

   // Velocity
   container.Insert(gv_zeros);

   // Magnetic field
   container.Insert(gv_zeros);

   // Effective "mesh" resolution
   double dmax = GSL_CONST_CGSM_ASTRONOMICAL_UNIT / unit_length_fluid;
   container.Insert(dmax);

   background.SetupObject(container);

   //--------------------------------------------------------------------------------------------------
   if (mpi_config->is_boss)
   {
      while (active_local_workers)
      {
         workers_stopped = server_back->ServerFunctions();
         active_local_workers -= workers_stopped;
      };
      server_back->ServerFinish();
   }
   else if (mpi_config->is_worker)
   {

      int i, j, k, N = 100;
      double Rs = 6.957e+10 / unit_length_fluid;
      double one_au = GSL_CONST_CGSM_ASTRONOMICAL_UNIT / unit_length_fluid;
      double x_min = -200.0 * one_au;
      double y_min = -200.0 * one_au;
      double z_min = -200.0 * one_au;
      double dx = 400.0 * one_au / (N - 1);
      double dy = 400.0 * one_au / (N - 1);
      double dz = 400.0 * one_au / (N - 1);
      double polarity, r_L;
      GeoVector drift_vel;
      spdata._mask = BACKGROUND_ALL | BACKGROUND_gradB;

      pos[0] = one_au * 40.0;
      mom[0] = Mom(1000.0 * SPC_CONST_CGSM_MEGA_ELECTRON_VOLT / unit_energy_particle, specie);
      vel[0] = Vel(mom[0], specie);
      background.GetFields(t, pos, mom, spdata);
      r_L = LarmorRadius(mom[0], spdata.Bmag, specie);
      std::cout << "|B| @ 1au = "
                << std::setw(18) << spdata.Bmag * unit_magnetic_fluid * 1.0e5 << " nT"
                << std::endl;
      std::cout << "rL (1 GeV) = "
                << std::setw(18) << r_L << " au"
                << std::endl;

      //--------------------------------------------------------------------------------------------------
      std::cout << "3D plots..." << std::endl;
      data_file.open("output/data_file_.dat");

      pos = gv_zeros;
      for (i = 0; i < N; i++)
      {
         for (j = 0; j < N; j++)
         {
            for (k = 0; k < N; k++)
            {
               pos[0] = x_min + i * dx;
               pos[1] = y_min + j * dy;
               pos[2] = z_min + k * dz;

               if (pos.Norm() < 31.0 * one_au)
               {
                  spdata.n_dens = 0.0;
                  spdata.Uvec = gv_zeros;
                  spdata.Bmag = 0.0;
                  polarity = 0.0;
                  spdata.dmax = 0.0;
                  drift_vel = gv_zeros;
                  spdata.region = gv_zeros;
               }
               else
               {
                  background.GetFields(t, pos, mom, spdata);
                  polarity = (spdata.Bvec * pos >= 0.0 ? 1.0 : -1.0);
                  r_L = LarmorRadius(mom[0], spdata.Bmag, specie);
                  drift_vel = drift_numer(r_L, vel[0], spdata, specie);
               };

               data_file << std::setw(18) << pos[0] * unit_length_fluid / GSL_CONST_CGSM_ASTRONOMICAL_UNIT
                         << std::setw(18) << pos[1] * unit_length_fluid / GSL_CONST_CGSM_ASTRONOMICAL_UNIT
                         << std::setw(18) << pos[2] * unit_length_fluid / GSL_CONST_CGSM_ASTRONOMICAL_UNIT
                         << std::setw(18) << spdata.n_dens * unit_density_fluid
                         << std::setw(18) << spdata.Uvec[0] * unit_velocity_fluid
                         << std::setw(18) << spdata.Uvec[1] * unit_velocity_fluid
                         << std::setw(18) << spdata.Uvec[2] * unit_velocity_fluid
                         << std::setw(18) << spdata.Bvec[0] * unit_magnetic_fluid
                         << std::setw(18) << spdata.Bvec[1] * unit_magnetic_fluid
                         << std::setw(18) << spdata.Bvec[2] * unit_magnetic_fluid
                         << std::setw(18) << polarity
                         << std::setw(18) << spdata.dmax
                         << std::setw(18) << drift_vel.Norm() / vel[0]
                         << std::setw(18) << spdata.region[0]
                         << std::setw(18) << spdata.region[1]
                         << std::setw(18) << spdata.region[2]
                         << std::setw(18) << spdata.region[3]
                         << std::endl;
            };
         };
      };
      data_file.close();

      //--------------------------------------------------------------------------------------------------
      std::cout << "2D plots..." << std::endl;

      Den_file.open("output/den_equ_.dat");
      AbsVel_file.open("output/vel_equ_.dat");
      AbsMag_file.open("output/mag_equ_.dat");
      PolMag_file.open("output/pol_equ_.dat");
      dmax_file.open("output/dmax_equ_.dat");
      drift_file.open("output/drift_equ_.dat");
      HetFlx_file.open("output/het_flx_equ_.dat");
      TurEnr_file.open("output/tur_enr_equ_.dat");

      pos = gv_zeros;
      for (i = 0; i < N; i++)
      {
         pos[0] = x_min + i * dx;
         for (j = 0; j < N; j++)
         {
            pos[1] = y_min + j * dy;
            if (pos.Norm() < 31.0 * one_au)
            {
               spdata.n_dens = 0.0;
               spdata.Uvec = gv_zeros;
               spdata.Bmag = 0.0;
               polarity = 0.0;
               spdata.dmax = 0.0;
               drift_vel = gv_zeros;
               spdata.region = gv_zeros;
            }
            else
            {
               background.GetFields(t, pos, mom, spdata);
               polarity = (spdata.Bvec * pos >= 0.0 ? 1.0 : -1.0);
               r_L = LarmorRadius(mom[0], spdata.Bmag, specie);
               drift_vel = drift_numer(r_L, vel[0], spdata, specie);
            };
            Den_file << std::setw(18) << spdata.n_dens * unit_density_fluid;
            AbsVel_file << std::setw(18) << spdata.Uvec.Norm() * unit_velocity_fluid;
            AbsMag_file << std::setw(18) << spdata.Bmag * unit_magnetic_fluid;
            PolMag_file << std::setw(18) << polarity;
            dmax_file << std::setw(18) << spdata.dmax;
            drift_file << std::setw(18) << drift_vel.Norm() / vel[0];
            HetFlx_file << std::setw(18) << spdata.region[0];
            TurEnr_file << std::setw(18) << spdata.region[1];
         };
         Den_file << std::endl;
         AbsVel_file << std::endl;
         AbsMag_file << std::endl;
         PolMag_file << std::endl;
         dmax_file << std::endl;
         drift_file << std::endl;
         HetFlx_file << std::endl;
         TurEnr_file << std::endl;
      };
      Den_file.close();
      AbsVel_file.close();
      AbsMag_file.close();
      PolMag_file.close();
      dmax_file.close();
      drift_file.close();
      HetFlx_file.close();
      TurEnr_file.close();

      Den_file.open("output/den_mer_.dat");
      AbsVel_file.open("output/vel_mer_.dat");
      AbsMag_file.open("output/mag_mer_.dat");
      PolMag_file.open("output/pol_mer_.dat");
      dmax_file.open("output/dmax_mer_.dat");
      drift_file.open("output/drift_mer_.dat");
      HetFlx_file.open("output/het_flx_mer_.dat");
      TurEnr_file.open("output/tur_enr_mer_.dat");

      pos = gv_zeros;
      for (i = 0; i < N; i++)
      {
         pos[0] = x_min + i * dx;
         for (k = 0; k < N; k++)
         {
            pos[2] = z_min + k * dz;
            if (pos.Norm() < 31.0 * one_au)
            {
               spdata.n_dens = 0.0;
               spdata.Uvec = gv_zeros;
               spdata.Bmag = 0.0;
               polarity = 0.0;
               spdata.dmax = 0.0;
               drift_vel = gv_zeros;
               spdata.region = gv_zeros;
            }
            else
            {
               background.GetFields(t, pos, mom, spdata);
               polarity = (spdata.Bvec * pos >= 0.0 ? 1.0 : -1.0);
               r_L = LarmorRadius(mom[0], spdata.Bmag, specie);
               drift_vel = drift_numer(r_L, vel[0], spdata, specie);
            };
            Den_file << std::setw(18) << spdata.n_dens * unit_density_fluid;
            AbsVel_file << std::setw(18) << spdata.Uvec.Norm() * unit_velocity_fluid;
            AbsMag_file << std::setw(18) << spdata.Bmag * unit_magnetic_fluid;
            PolMag_file << std::setw(18) << polarity;
            dmax_file << std::setw(18) << spdata.dmax;
            drift_file << std::setw(18) << drift_vel.Norm() / vel[0];
            HetFlx_file << std::setw(18) << spdata.region[0];
            TurEnr_file << std::setw(18) << spdata.region[1];
         };
         Den_file << std::endl;
         AbsVel_file << std::endl;
         AbsMag_file << std::endl;
         PolMag_file << std::endl;
         dmax_file << std::endl;
         drift_file << std::endl;
         HetFlx_file << std::endl;
         TurEnr_file << std::endl;
      };
      Den_file.close();
      AbsVel_file.close();
      AbsMag_file.close();
      PolMag_file.close();
      dmax_file.close();
      drift_file.close();
      HetFlx_file.close();
      TurEnr_file.close();

      //--------------------------------------------------------------------------------------------------

      //       std::cout << "1D plots..." << std::endl;

      //       Den_file.open("output/den_1au_.dat");
      //       AbsVel_file.open("output/vel_1au_.dat");
      //       AbsMag_file.open("output/mag_1au_.dat");
      //       PolMag_file.open("output/pol_1au_.dat");
      //       dmax_file.open("output/dmax_1au_.dat");
      //       drift_file.open("output/drift_1au_.dat");
      //       HetFlx_file.open("output/het_flx_1au_.dat");
      //       TurEnr_file.open("output/tur_enr_1au_.dat");

      //       double phi;
      //       double dphi = M_2PI / N;
      //       double rot_freq = 27.0 / M_2PI;

      //       pos = gv_zeros;
      //       for (i = 0; i < N; i++) {
      // // Frame rotates CCW in the xy-plane, so steady-state data should be sampled CW
      //          phi = M_PI_4 - i * dphi;
      //          pos[0] = one_au * cos(phi);
      //          pos[1] = one_au * sin(phi);

      //          background.GetFields(t, pos, mom, spdata);
      //          polarity = (spdata.Bvec * pos >= 0.0 ? 1.0 : -1.0);
      //          r_L = LarmorRadius(mom[0], spdata.Bmag, specie);
      //          drift_vel = drift_numer(r_L, vel[0], spdata, specie);

      //          Den_file << std::setw(18) << i * dphi * rot_freq
      //                   << std::setw(18) << spdata.n_dens * unit_density_fluid
      //                   << std::endl;
      //          AbsVel_file << std::setw(18) << i * dphi * rot_freq
      //                      << std::setw(18) << spdata.Uvec.Norm() * unit_velocity_fluid
      //                      << std::endl;
      //          AbsMag_file << std::setw(18) << i * dphi * rot_freq
      //                      << std::setw(18) << spdata.Bmag * unit_magnetic_fluid
      //                      << std::endl;
      //          PolMag_file << std::setw(18) << i * dphi * rot_freq
      //                      << std::setw(18) << polarity
      //                      << std::endl;
      //          dmax_file << std::setw(18) << i * dphi * rot_freq
      //                    << std::setw(18) << spdata.dmax
      //                    << std::endl;
      //          drift_file << std::setw(18) << i * dphi * rot_freq
      //                     << std::setw(18) << drift_vel.Norm() / vel[0]
      //                     << std::endl;
      //          HetFlx_file << std::setw(18) << i * dphi * rot_freq
      //                      << std::setw(18) << spdata.region[0]
      //                      << std::endl;
      //          TurEnr_file << std::setw(18) << i * dphi * rot_freq
      //                      << std::setw(18) << spdata.region[1]+spdata.region[2]
      //                      << std::endl;
      //       };

      //       Den_file.close();
      //       AbsVel_file.close();
      //       AbsMag_file.close();
      //       PolMag_file.close();
      //       dmax_file.close();
      //       drift_file.close();
      //       HetFlx_file.close();
      //       TurEnr_file.close();

      background.StopServerFront();
      std::cout << "Background samples outputted to file." << std::endl;
   };

   std::cout << "Node " << mpi_config->glob_comm_rank << " exited." << std::endl;
   return 0;
};