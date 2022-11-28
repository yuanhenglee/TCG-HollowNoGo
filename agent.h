/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include <assert.h>
#include <chrono>
#include "board.h"
#include "action.h"

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

class Node {
public:
	Node( size_t who ) : parent(nullptr), who(who), pos(), children() {}
	Node( size_t who, board::point pos ) : parent(nullptr), who(who), pos(pos), children() {}
	Node(Node* parent, size_t who, board::point pos ) : parent(parent), who(who), pos(pos), children() {}
	~Node() {
		for (auto& child : children)
			delete child;
	}
	const Node* get_parent() const { return parent; };

	Node* getBestChild() {
		// std::vector<double> scores;
		Node* bestChild = nullptr;
		double max_score = -1;
		double log2visits = log2( visits );
		for (auto& child : children) {
			// selection with standard UCB
			if (child->visits == 0){
			 	child->ucb = std::numeric_limits<double>::max();
				return child;
			}
			else{
				child->ucb = child->wins / child->visits + 0.25 * sqrt(log2visits / child->visits);
			}

			if( child->ucb > max_score ){
				max_score = child->ucb;
				bestChild = child;
			}
			
			// scores.emplace_back(child->ucb);
		}
		return bestChild;
		// // find the best score
		// double best_score = *std::max_element(scores.begin(), scores.end());

		// // TRY: randomly select from the best children
		// std::vector<Node*> best_children;
		// // for( size_t i = 0; i < scores.size(); i++ ) {
		// for( auto& child : children) {
		// 	if ( best_score - child->ucb < 1e-6 ) {
		// 		best_children.emplace_back(child);
		// 	}
		// }

		// // return best_child;
		// return best_children[rand() % best_children.size()];
	}

	bool expand(const board& state) {
		// std::cout<<"expanding "<<pos<<std::endl;

		// expand the node if it is not a leaf
		if( visits == 0 || is_leaf ) return false;
		

		// if the node is expanded, skip the expansion process
		if( is_expanded )
			return true;

		board after = board(state);

		// get all possible actions
		std::vector<board::point> points = after.get_legal_pts(); 

		// std::cout<<"points.size() : "<<points.size()<<std::endl;
		// return false if there is no possible action
		if (points.empty()){
			is_leaf = true;
			return false;
		}

		// expand children
		for (auto& point : points) {
			Node* child = new Node( this, 3u-who, point);
			// std::cout<<"child:"<<child->pos<<", parent: "<<child->parent->pos<<std::endl;
			children.emplace_back(child);
		}

		// shuffle children vector index
		// std::shuffle(children.begin(), children.end(), std::default_random_engine());
		
		// update expanded status
		is_expanded = true;

		return true;
	}

	Node* traverse( board& state ) {
		Node* node = this;
		while( node->children.size() > 0 ){
		// while( node->is_fully_expanded() ){
			node = node->getBestChild();
			assert(state.place(node->pos) == board::legal);
		}
		return node;
	}

	Node* treePolicy(board& state) {
		//selection
		Node* cur = this->traverse(state);

		//expansion
		if(cur->expand(state)){
			cur = cur->getBestChild();
			assert(state.place( cur->pos ) == board::legal);
		}
		return cur;
	}

	size_t defaultPolicy( const board& state ) {
		board after  = board(state);
		size_t cur_who = 3u-who;
		while ( true ) {
			// std::cout<<state<<std::endl;
			board::point point = after.get_random_legal_pt( );
			// std::cout<<"default policy : "<<point.x<<","<<point.y<<std::endl;
			if( point.x == -1 && point.y == -1 ) return 3u-cur_who;
			after.place( point.x, point.y );
			cur_who = 3u-cur_who;
		}
	}
	

	void backPropagate( size_t winner) {
		// back propagate the result till the root
		Node* node = this;
		while (node != nullptr) {
			node->visits++;
			node->wins += winner == node->who? 1 : 0;
			node = node->parent;
		}
	}

	// bool is_fully_expanded() const {
	// 	return is_expanded && expanded_count == children.size();
	// }

	Node* parent = nullptr;
	double visits = 0;
	double wins = 0;
	double ucb = 0;
	// long unsigned int expanded_count = 0;
	size_t who;
	board::point pos;
	std::vector<Node*> children;
	bool is_leaf = false, is_expanded = false;
};

/**
 * player for both side
 * random: put a legal piece randomly
 * mcts: use mcts to find the best move
 */
class player : public random_agent {
   public:
    player(const std::string& args = "")
        : random_agent("name=random role=unknown " + args), space(board::size_x * board::size_y), who(board::empty) {
        if (name().find_first_of("[]():; ") != std::string::npos)
            throw std::invalid_argument("invalid name: " + name());
        if (role() == "black") who = board::black;
        if (role() == "white") who = board::white;
        if (who == board::empty)
            throw std::invalid_argument("invalid role: " + role());

		if( args.find("mcts") != std::string::npos ) {
			method = "mcts";
			if (meta.find("T") != meta.end())
				T = int(meta["T"]);
			if (meta.find("time") != meta.end())
				t_limit = int(meta["time"]);
			if (meta.find("debug") != meta.end())
				debug = bool(meta["debug"]);
		}
		else{
			for (size_t i = 0; i < space.size(); i++)
				space[i] = action::place(i, who);
		}

    }
	
	// just for test
	void print_tree( Node* root, int depth ){
		if( root == nullptr || depth > 2 ) return;
		for( int i = 0; i < depth; i++ ) std::cout<<"  ";
		std::cout<<(root->who==1?"B:":"W:")<<root->pos<<"\t"<<root->wins<<"/"<<root->visits<<"\t"<<root->ucb<<root->children.size()<<std::endl;
		for( auto& child : root->children ){
			print_tree( child, depth+1 );
		}
	}

    virtual action take_action(const board& state) {
		if( method == "mcts" )
			return mcts_action(state);
		else
			return random_action(state);
	}

	action random_action(const board& state) {
		std::shuffle(space.begin(), space.end(), engine);
		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal)
				return move;
		}
		return action();
	}

    action mcts_action(const board& state) {
		
		const auto time_limit = std::chrono::milliseconds(t_limit);
		const auto start_time = std::chrono::high_resolution_clock::now();
		
		Node* root = new Node( 3u-who, board::point(-1, -1) );
		for( int i = 0; i < T ; i++ ) {

			board after = board(state);

			// find the best node to expand
			Node* expand_node = root->treePolicy( after );

			// random run to add node and get reward
			size_t winner = expand_node->defaultPolicy( after );

			// update all passing nodes with reward
			expand_node->backPropagate( winner );

			if( i > 0.2*T && std::chrono::high_resolution_clock::now() - start_time > time_limit ){
				if( debug )
					std::cout<<"time limit reached i = "<<i<<std::endl;
				break;
			}

		}

		// get the best child
		Node* best_child = nullptr;
		for( auto& child : root->children ){
			if( best_child == nullptr || child->visits > best_child->visits ){
				best_child = child;
			}
		}

		if( debug ){
			std::cout<<"-----------------"<<std::endl;
			std::cout<<state<<std::endl;
			print_tree(root, 0);
		}

		if( best_child == nullptr ){
			if( debug ) std::cout<<"best child is null"<<std::endl;
			delete root;
			return action();
		}

		if( debug ) std::cout<<"best child : "<<best_child->pos<<std::endl;
		action::place move =  action::place(best_child->pos.x, best_child->pos.y, who);
		delete root;
		return move;
    }

   private:
    std::vector<action::place> space;
	std::string method = "random";
    board::piece_type who;
	int T = 10000, t_limit = 1300;
	bool debug = false;
};

