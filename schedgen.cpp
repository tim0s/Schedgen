/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Torsten Hoefler <htor@cs.indiana.edu>
 *            Timo Schneider <timoschn@cs.indiana.edu>
 *
 */

#include <sstream>
#include <fstream>
#include <algorithm>
#include <queue>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MersenneTwister.h"
#include "schedgen.hpp"


#define RANK2VRANK(rank, vrank, root) \
{ \
  vrank = rank; \
  if (rank == 0) vrank = root; \
  if (rank == root) vrank = 0; \
}                                                                                                                                                                                                                                                        
#define VRANK2RANK(rank, vrank, root) \
{ \
  rank = vrank; \
  if (vrank == 0) rank = root; \
  if (vrank == root) rank = 0; \
} 

Goal::Goal(gengetopt_args_info *args_info, int nranks) : sends(0), recvs(0), execs(0), ranks(0), reqs(0), curtag(-1) {
	// create filename
	this->filename.clear();
	this->filename.append(std::string(args_info->filename_arg));
	this->myfile.open(this->filename.c_str(), std::fstream::out | std::fstream::trunc);
	std::stringstream tmp;
	tmp << "num_ranks " << nranks << "\n";
  	this->AppendString(tmp.str());

  this->cpu=args_info->cpu_arg;
  this->nb=args_info->nb_given;
  if(nb) {
    this->poll_int=args_info->nb_poll_arg;
    this->nbfunc=args_info->nb_arg;
    this->ranks_init.resize(nranks);
    std::fill(this->ranks_init.begin(), this->ranks_init.end(), false);
  }
}

Goal::~Goal() {

	if (myfile.is_open()) {
		myfile.close();
	}
	else {
		std::cout << "Unable to open file " << this->filename;
		std::cout << " for writing" << std::endl;
	}

  std::cout << sends << " sends, " << recvs << " recvs, " << execs << " execs, and " << reqs << " reqs among " << ranks << " hosts written" << std::endl;


}

void Goal::StartRank(int rank) {

  this->ranks++;
  this->curtag = 0;
	
	std::stringstream tmp;
	tmp << "\nrank " << rank << " {\n";
	AppendString(tmp.str());

	// reset label counter
	this->id_counter = 0;

  if(nb) {
    if(!ranks_init[rank]) {
      if(poll_int) { // do we segment?
        int last=0;
        for(int i=0; i<nbfunc; i+=poll_int) {
          int cur=this->Exec("nbfunc", poll_int, cpu);
          // no last in first round :)
          if(i) this->Requires(cur, last);
          last = cur;
        }
      } else { // no segmentation
        this->Exec("nbfunc", nbfunc, cpu);
      }
    }
    ranks_init[rank] = true;
  }
}

Goal::t_id Goal::Send(int bufsize, int dest) {

  this->sends++;
	
	std::stringstream tmp;
	
	this->id_counter++;
	tmp << "l" << this->id_counter << ": ";
	tmp << "send ";
	tmp << bufsize << "b ";
	tmp << "to " << dest << " tag " << curtag;
  	tmp << std::endl;
	AppendString(tmp.str());

	// append to independent set
	start.insert(id_counter);
	end.insert(id_counter);
	
	return this->id_counter;
}

void Goal::Comment(std::string comment) {
	std::stringstream tmp;
	tmp << "/* " << comment << " */" << std::endl;
	AppendString(tmp.str());
}

Goal::t_id Goal::Recv(int bufsize, int src) {

  this->recvs++;

	std::stringstream tmp;
	
	this->id_counter++;
	tmp << "l" << this->id_counter << ": ";
	tmp << "recv ";
	tmp << bufsize << "b ";
	tmp << "from " << src << " tag " << curtag;
  	tmp << std::endl;
	AppendString(tmp.str());
	
	// append to independent set
	start.insert(id_counter);
	end.insert(id_counter);

	return this->id_counter;
}

void Goal::Requires(Goal::t_id tail, Goal::t_id head) {

  this->reqs++;
	
	std::stringstream tmp;

	tmp << "l" << tail << " requires ";
	tmp << "l" << head << std::endl;
  
	// erase from independent set
	start.erase(tail);
	end.erase(head);
	
	AppendString(tmp.str());
}

void Goal::Irequires(int tail, int head) {

  this->reqs++;
	
	std::stringstream tmp;

	tmp << "l" << tail << " irequires ";
	tmp << "l" << head << std::endl;
	
	// append to independent set
	start.erase(tail);
	end.erase(head);
	
	AppendString(tmp.str());
}

void Goal::EndRank() {
	
	AppendString("}\n");
}

void Goal::AppendString(std::string str) {
	this->schedule.append(str);
	
	if (this->schedule.length() > 1024*1024*16) {
		// write the schedule if it is bigger than 16 MB
		this->Write();
	}
}

void Goal::Write() {
	
	if (myfile.is_open()) {
		myfile << this->schedule;
		this->schedule.clear();
		myfile.sync();
	}
	else {
		std::cout << "Unable to open file " << this->filename;
		std::cout << " for writing" << std::endl;
	}

}

// this exec stuff is a big mess and needs to be cleaned up sometime ...
int Goal::Exec(std::string opname, btime_t size, int proc) {

  this->execs++;

	std::stringstream tmp;
	
	this->id_counter++;
	tmp << "l" << this->id_counter << ": ";
	tmp << "calc " << size ;
	if(cpu) tmp << " cpu " << proc; 
	tmp << std::endl; 
	this->schedule.append(tmp.str());
	
	return this->id_counter;
}

int Goal::Exec(std::string opname, std::vector<buffer_element> buf) {

  this->execs++;

	std::stringstream tmp;
	
	this->id_counter++;
	tmp << "l" << this->id_counter << ": ";
	tmp << "calc "; 
  int size=0;
	for (unsigned int i = 0; i < buf.size(); i++) {
		size += buf[i].size;
	}
	tmp << size << std::endl;
	this->schedule.append(tmp.str());
	
	return this->id_counter;
}

int Goal::Exec(std::string opname, btime_t size) {

	std::vector<buffer_element> buf;
	buffer_element elem = buffer_element(1, 1, size);
	buf.push_back(elem);
	return Exec(opname, buf);
}

void create_binomial_tree_bcast_rank(Goal *goal, int root, int comm_rank, int comm_size, int datasize) {
	int vrank;
	RANK2VRANK(comm_rank, vrank, root);

	Goal::t_id recv = -1, send = -1;
	for (int r = 0; r < ceil(log2(comm_size)); r++) {
		int vpeer =  vrank+(int)pow(2,r);
		int peer;
		VRANK2RANK(peer, vpeer, root);
		if ((vrank+pow(2,r) < comm_size) and (vrank < pow(2,r))) {
			send = goal->Send(datasize, peer);
		}
		if ((send >= 0) and (recv >= 0)) {
			goal->Requires(send, recv);
		}
		vpeer = vrank-(int)pow(2, r);
		VRANK2RANK(peer, vpeer, root);
		if ((vrank >= pow(2,r)) and (vrank < pow(2, r+1))) {
			recv = goal->Recv(datasize, peer);	
		}
	}
}

void create_binomial_tree_bcast(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	Goal goal(args_info, comm_size);
	
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
	  goal.StartRank(comm_rank);
    create_binomial_tree_bcast_rank(&goal, args_info->root_arg, comm_rank, comm_size, datasize);
	  goal.EndRank();
	} 
	goal.Write();
}


void create_binary_tree_bcast_rank(Goal *goal, int root, int comm_rank, int comm_size, int datasize) {
	int vrank;
	RANK2VRANK(comm_rank, vrank, root);

  std::queue<int> q;
  int cnt = 0;
  q.push(0);

  const int GOAL_ID_NULL=-1;
  Goal::t_id recv = GOAL_ID_NULL;
  Goal::t_id send[2] = { GOAL_ID_NULL, GOAL_ID_NULL };
  while(cnt < comm_size) {
    int src=q.front(); q.pop();
    int tgt=++cnt;
    int vtgt, vsrc;
		VRANK2RANK(vtgt, tgt, root);
		VRANK2RANK(vsrc, src, root);
    if(tgt < comm_size) {
      if(comm_rank == src) {
        if(send[0] == GOAL_ID_NULL) send[0] = goal->Send(datasize, vtgt);
        else if(send[1] == GOAL_ID_NULL) send[1] = goal->Send(datasize, vtgt);
        else assert(false);
      }
      if(comm_rank == tgt) {
        assert(recv == GOAL_ID_NULL);
        recv =  goal->Recv(datasize, vsrc);
      }
      q.push(tgt);
    }
    tgt = ++cnt;
		VRANK2RANK(vtgt, tgt, root);
    if(tgt < comm_size) {
      if(comm_rank == src) {
        if(send[0] == GOAL_ID_NULL) send[0] = goal->Send(datasize, vtgt);
        else if(send[1] == GOAL_ID_NULL) send[1] = goal->Send(datasize, vtgt);
        else assert(false);
      }
      if(comm_rank == tgt) {
        assert(recv == GOAL_ID_NULL);
        recv =  goal->Recv(datasize, vsrc);
      }
      q.push(tgt);
    }
  }

  if(comm_rank > 0) {
    if(recv != GOAL_ID_NULL) {
      if(send[0] != GOAL_ID_NULL) goal->Requires(send[0], recv);
      if(send[1] != GOAL_ID_NULL) goal->Requires(send[1], recv);
    }
  }

  /*

	Goal::t_id recv = -1, send = -1;
	for (int r = 0; r < ceil(log2(comm_size)); r++) {
		int vpeer =  vrank+(int)pow(2,r);
		int peer;
		VRANK2RANK(peer, vpeer, root);
		if ((vrank+pow(2,r) < comm_size) and (vrank < pow(2,r))) {
			send = goal->Send(datasize, peer);
		}
		if ((send >= 0) and (recv >= 0)) {
			goal->Requires(send, recv);
		}
		vpeer = vrank-(int)pow(2, r);
		VRANK2RANK(peer, vpeer, root);
		if ((vrank >= pow(2,r)) and (vrank < pow(2, r+1))) {
			recv = goal->Recv(datasize, peer);	
		}
	}*/
}


void create_binary_tree_bcast(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	Goal goal(args_info, comm_size);
	
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
	  goal.StartRank(comm_rank);
    create_binary_tree_bcast_rank(&goal, args_info->root_arg, comm_rank, comm_size, datasize);
	  goal.EndRank();
	} 
	goal.Write();
}

void create_dissemination_rank(Goal *goal, int comm_rank, int comm_size, int datasize) {
  int dist = 1<<0;

	Goal::t_id recv = -1, send = -1;
  while(dist<comm_size) {
  	send = goal->Send(datasize, (comm_rank+dist+comm_size)%comm_size);	
    if(recv != -1) goal->Requires(send, recv);
  	recv = goal->Recv(datasize, (comm_rank-dist+comm_size)%comm_size);	

    dist <<= 1;
  }
}

void create_dissemination(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	Goal goal(args_info, comm_size);
	
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
	  goal.StartRank(comm_rank);
    create_dissemination_rank(&goal, comm_rank, comm_size, datasize);
	  goal.EndRank();
	} 
	goal.Write();
}

void create_binomial_tree_reduce_rank(Goal *goal, int root, int comm_rank, int comm_size, int datasize) {
	int vrank;
	RANK2VRANK(comm_rank, vrank, root);
	
	Goal::t_id send = -1, recv = -1;
	for (int r = 0; r < ceil(log2(comm_size)); r++) {
		int vpeer =  vrank+(int)pow(2,r);
		int peer;
		VRANK2RANK(peer, vpeer, root);
		if ((vrank+pow(2,r) < comm_size) and (vrank < pow(2,r))) {
			recv = goal->Recv(datasize, peer);
		}
		if ((send >= 0) and (recv >= 0)) {
			goal->Requires(send, recv);
		}
		vpeer = vrank-(int)pow(2, r);
		VRANK2RANK(peer, vpeer, root);
		if ((vrank >= pow(2,r)) and (vrank < pow(2, r+1))) {
			send = goal->Send(datasize, peer);
		}
	} 
}

void create_binomial_tree_reduce(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;

	Goal goal(args_info, comm_size);

	
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);
    create_binomial_tree_reduce_rank(&goal, args_info->root_arg, comm_rank, comm_size, datasize);
		goal.EndRank();
	} 
	goal.Write();
}

void create_pipelined_ring(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	int segmentsize = args_info->segmentsize_arg;

	Goal goal(args_info, comm_size);
	
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);
		int vrank;
		RANK2VRANK(comm_rank, vrank, args_info->root_arg);

		int num_rounds = (int)ceil((double) datasize / (double) segmentsize);
	
		std::vector<int> send(num_rounds, -1);
		std::vector<int> recv(num_rounds, -1);
	
		for ( int r = 0; r < num_rounds; r++ ) {
	
			int recvpeer = vrank - 1;
			int sendpeer = vrank + 1;

			// how much data is transmitted in this round
			int psize = segmentsize;
			if (r == num_rounds-1) {
				psize = datasize - segmentsize * r;
			}
			
			// recv (if we are a receiver)
			int vpeer = recvpeer;
			VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
			if (recvpeer >= 0) {
				recv.at(r) = goal.Recv(psize, recvpeer);
			}
			
			// send (if we are a sender)
			vpeer = sendpeer;
			VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
			if (sendpeer < comm_size) {
				send.at(r) = goal.Send(psize, sendpeer);
			}
			
			// we can not send data before we received it	
			if ((send.at(r)>0) and (recv.at(r)>0)) {
				goal.Requires(send.at(r), recv.at(r));
			}
		}
		goal.EndRank();
		if (comm_rank ==  comm_size-1) goal.Write();
	} 
}

void create_scatter(gengetopt_args_info *args_info) {
	
	/**
   * In this pattern the root sends data to every other rank.
	 */

	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	
	Goal goal(args_info, comm_size);

	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);
		int vrank;
		RANK2VRANK(comm_rank, vrank, args_info->root_arg);
			
		if (vrank == 0) {
			for (int i=1; i<comm_size; i++) {
				int recvpeer = i;
				int vpeer = recvpeer;
				VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
				goal.Send(datasize, recvpeer);
			}		
		}
		else {
			int sendpeer = 0;
			int vpeer = sendpeer;
			VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
			goal.Recv(datasize, sendpeer);		
		}
		goal.EndRank();	
	}
	goal.Write(); 
}

void create_gather(gengetopt_args_info *args_info) {
	
	/**
	 * In this pattern every rank (except root) sends data to the root, which has to
	 * receive it. Altough in MPI_Gather root sends data to himself, we are not doing
	 * it that way here, because one can assume that a good MPI imnplementation would
	 * just copy the data locally.
	 */

	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	
	Goal goal(args_info, comm_size);

	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);
		int vrank;
		RANK2VRANK(comm_rank, vrank, args_info->root_arg);
			
		if (vrank == 0) {
			for (int i=1; i<comm_size; i++) {
				int recvpeer = i;
				int vpeer = recvpeer;
				VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
				goal.Recv(datasize, recvpeer);
			}		
		}
		else {
			int sendpeer = 0;
			int vpeer = sendpeer;
			VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
			goal.Send(datasize, sendpeer);		
		}
		goal.EndRank();	
	}
	goal.Write(); 
}

void create_pipelined_ring_dep(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	int segmentsize = args_info->segmentsize_arg;
	
	Goal goal(args_info, comm_size);

	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);
		int vrank;
		RANK2VRANK(comm_rank, vrank, args_info->root_arg);

		int num_rounds = (int)ceil((double) datasize / (double) segmentsize);
	
		std::vector<int> send(num_rounds, -1);
		std::vector<int> recv(num_rounds, -1);
	
		for ( int r = 0; r < num_rounds; r++ ) {
		
			int recvpeer = vrank - 1;
			int sendpeer = vrank + 1;

			// how much data is transmitted in this round
			int psize = segmentsize;
			if (r == num_rounds-1) {
				psize = datasize - segmentsize * r;
			}
			
			// recv (if we are a receiver)
			int vpeer = recvpeer;
			VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
			if (vpeer >= 0) {
				recv.at(r) = goal.Recv(psize, recvpeer);
			}
			
			// send (if we are a sender)
			vpeer = sendpeer;
			VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
			if (vpeer < comm_size) {
				send.at(r) = goal.Send(psize, sendpeer);
			}
			
			// we can not send data before we received it	
			if ((send.at(r)>0) and (recv.at(r)>0)) {
				goal.Requires(send.at(r), recv.at(r));
			}
			
			// ensure pipelining (only receive round r after r-1 send finished)
			if ((r>0) and (send.at(r-1)>0) and (recv.at(r)>0)) {
				goal.Requires(recv.at(r), send.at(r-1));
			}

		}
		goal.EndRank();
		if (comm_rank ==  comm_size-1) goal.Write();
	} 
}

double mylog(double base, double x) {
	return log(x)/log(base);		
}

int max(int a, int b) {
	if (a>b) return a;
	return b;
}

int mymod(int a, int b) {
	// calculate a % b
	
	while (a<0) {
		a += b;		
	}
	return a%b;
}

void create_nway_dissemination(gengetopt_args_info *args_info) {
	
	int n = args_info->nway_arg;
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;

	Goal goal(args_info, comm_size);

	
	for (int comm_rank=0; comm_rank<comm_size; comm_rank++) {
		
		goal.StartRank(comm_rank);
		
		std::vector<int> recv(n,-1);
		std::vector<int> send(n,-1);
	
		for (int r=0; r < ceil( mylog(n+1, comm_size) ); r++) {
			for (int w=1; w<=n; w++) {
				int sendpeer =  mymod( comm_rank + w * (int)pow(n+1, r), comm_size );
				send[w-1] = goal.Send(datasize, sendpeer);
			}
			if (r > 0) {
				int prev = recv[0];
				for (int w=1; w<n; w++) {
					int red = goal.Exec("redfunc", datasize);
					goal.Requires(red, recv[w]);
					goal.Requires(red, prev);
					prev = red;
				}
				int red = goal.Exec("redfunc", datasize);
				goal.Requires(red, recv[0]);
				goal.Requires(send[n-1], prev);
				for (int w=1; w<=n; w++) {
					goal.Requires(send[w-1], red);
				}
			}
			for (int w=1; w<=n; w++) {
				int recvpeer =  mymod(comm_rank - w * (int)pow(n+1, r) ,  comm_size);
				recv[w-1] = goal.Recv(datasize, recvpeer);
			}
		}
		goal.EndRank();
	}
	goal.Write(); 

}

void create_double_ring(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	
	Goal goal(args_info, comm_size);

	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);

    /* forward ring */
	  int send = goal.Send(datasize, ((comm_rank+1)+comm_size)%comm_size);
		int recv = goal.Recv(datasize, ((comm_rank-1)+comm_size)%comm_size);	
    if(comm_rank > 0) goal.Requires(send, recv);

    /* backward ring */
	  send = goal.Send(datasize, ((comm_rank-1)+comm_size)%comm_size);
		recv = goal.Recv(datasize, ((comm_rank+1)+comm_size)%comm_size);	
    if(comm_rank > 0) goal.Requires(send, recv);

		goal.EndRank();
	} 
  goal.Write();
}

/* creates a linaer (central counter based) barrier algoritm */
void create_linbarrier(gengetopt_args_info *args_info) {
	

	int comm_size = args_info->commsize_arg;
	Goal goal(args_info, comm_size);
	
  // each rank
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
    // beware - this goal object is stateful! only one rank at a time!
		goal.StartRank(comm_rank);

    if(comm_rank == 0) {
      // dummy operation to reduce number of dependencies from n*n to n+n
			int dummy = goal.Exec("redfunc", 0);
	    for (int i = 1; i < comm_size; i++) {
        // rank 0 receives from all - and sends to all
		    int recv = goal.Recv(1, i);
	      int send = goal.Send(1, i);
        goal.Requires(dummy, recv);
        goal.Requires(send, dummy);
      }
    } else {
  	  goal.Send(1, 0);
	  	goal.Recv(1, 0);	
    }

    // beware - this is also stateful and finishes a rank!
		goal.EndRank();
	} 
  goal.Write();
}

void create_random_bisect(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	if (comm_size % 2 == 1) comm_size--;
	int datasize = args_info->datasize_arg;
	Goal goal(args_info, comm_size);
	
	MTRand mtrand;
	std::vector<int> peer(comm_size); // save the pairs (peer[i] is the peer of host i)
	std::vector<bool> used(comm_size, false); // mark the used peers 
	
	// quick method to create a random pairing
	for (int counter = 0; counter < comm_size; counter++) {
		int myrand = mtrand.randInt(comm_size-counter-1);
		int pos=0;
		while(true) {
			// walk the used array (only the entries that are not used)
			if(used[pos] == false) {
				if(myrand == 0) {
					used[pos] = true;
					peer[counter] = pos; // save random value
					break;
				}
				myrand--;
			}
			pos++;
			assert(pos < comm_size);
		}
	}

	// create the inverse array ...
	std::vector<int> inverse_peer(comm_size); // the inverse peer table (know who to receive from)
	for(int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		inverse_peer[peer[comm_rank]] = comm_rank;
	}
	
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);
		
		int dist = comm_size/2;

		if (inverse_peer[comm_rank] < dist) {
			// this host is a sender
			goal.Send(datasize, peer[inverse_peer[comm_rank]+dist]);
		}
		else {
			// this host is a receiver
			goal.Recv(datasize, peer[inverse_peer[comm_rank]-dist]);
		}

		goal.EndRank();
	} 
	goal.Write();
}

void create_random_bisect_fd_sym(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	if (comm_size % 2 == 1) comm_size--;
	int datasize = args_info->datasize_arg;
	Goal goal(args_info, comm_size);
	
	MTRand mtrand;
	std::vector<int> peer(comm_size); // save the pairs (peer[i] is the peer of host i)
	std::vector<bool> used(comm_size, false); // mark the used peers 
	
	// quick method to create a random pairing
	for (int counter = 0; counter < comm_size; counter++) {
		int myrand = mtrand.randInt(comm_size-counter-1);
		int pos=0;
		while(true) {
			// walk the used array (only the entries that are not used)
			if(used[pos] == false) {
				if(myrand == 0) {
					used[pos] = true;
					peer[counter] = pos; // save random value
					break;
				}
				myrand--;
			}
			pos++;
			assert(pos < comm_size);
		}
	}

	// create the inverse array ...
	std::vector<int> inverse_peer(comm_size); // the inverse peer table (know who to receive from)
	for(int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		inverse_peer[peer[comm_rank]] = comm_rank;
	}
	
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
		goal.StartRank(comm_rank);
		
		int dist = comm_size/2;

		if (inverse_peer[comm_rank] < dist) {
			// this host is a sender
			goal.Send(datasize, peer[inverse_peer[comm_rank]+dist]);
			goal.Recv(datasize, peer[inverse_peer[comm_rank]+dist]);
		}
		else {
			// this host is a receiver
			goal.Recv(datasize, peer[inverse_peer[comm_rank]-dist]);
			goal.Send(datasize, peer[inverse_peer[comm_rank]-dist]);
		}

		goal.EndRank();
	} 
	goal.Write();
}

void create_linear_alltoall_rank(Goal *goal, int src_rank, int comm_size, int datasize) {
 for (int dst_rank = 0; dst_rank < comm_size-1; dst_rank++) {
    goal->Send(datasize, (src_rank+1+dst_rank)%comm_size);
    goal->Recv(datasize, (src_rank-1-dst_rank+2*comm_size)%comm_size);	
  } 
}

void create_linear_alltoall(gengetopt_args_info *args_info) {
	
	int comm_size = args_info->commsize_arg;
	int datasize = args_info->datasize_arg;
	
	Goal goal(args_info, comm_size);

	for (int src_rank = 0; src_rank < comm_size; src_rank++) {
    goal.StartRank(src_rank);
    create_linear_alltoall_rank(&goal, src_rank, comm_size, datasize);
    goal.EndRank();
  }
  goal.Write();
}


int main(int argc, char **argv) {
	
	gengetopt_args_info args_info;
	
	if (cmdline_parser(argc, argv, &args_info) != 0) {
		fprintf(stderr, "Couldn't parse command line arguments!\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(args_info.ptrn_arg, "binarytreebcast") == 0) {
		 create_binary_tree_bcast(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "binomialtreebcast") == 0) {
		 create_binomial_tree_bcast(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "binomialtreereduce") == 0) {
		 create_binomial_tree_reduce(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "nwaydissemination") == 0) {
		 create_nway_dissemination(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "pipelinedring") == 0) {
		 create_pipelined_ring(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "pipelinedringdep") == 0) {
		 create_pipelined_ring_dep(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "doublering") == 0) {
		 create_double_ring(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "gather") == 0) {
		 create_gather(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "scatter") == 0) {
		 create_scatter(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "linbarrier") == 0) {
		 create_linbarrier(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "dissemination") == 0) {
		 create_dissemination(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "random_bisect") == 0) {
		 create_random_bisect(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "random_bisect_fd_sym") == 0) {
		 create_random_bisect_fd_sym(&args_info);
	}
	if (strcmp(args_info.ptrn_arg, "linear_alltoall") == 0) {
		 create_linear_alltoall(&args_info);
	}


	if (strcmp(args_info.ptrn_arg, "trace") == 0) {
    // see process_trace.cpp 
		process_trace(&args_info);
	}
	
	cmdline_parser_free(&args_info);
	exit(EXIT_SUCCESS);

}
