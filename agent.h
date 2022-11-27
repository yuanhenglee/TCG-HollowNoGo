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

/**
 * random player for both side
 * put a legal piece randomly
 */
class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) {
		std::shuffle(space.begin(), space.end(), engine);
		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal)
				return move;
		}
		return action();
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
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
		Node* best_child = nullptr;
		double best_score = -1;
		for (auto& child : children) {
			// selection with standard UCB
			if (child->visits == 0) 
				return child;

			double score = child->wins / child->visits + sqrt(2 * log(visits) / child->visits);
			if (score - best_score > 0.0001) {
				best_child = child;
				best_score = score;
			}
		}
		return best_child;
	}

	bool expand(const board& state) {
		// std::cout<<"expanding "<<pos<<std::endl;

		// expand the node if it is not a leaf
		if( is_leaf ) return false;
		

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
		
		// update expanded status
		is_expanded = true;

		return true;
	}

	Node* traverse( board& state ) {
		Node* node = this;
		while( node->is_fully_expanded() ){
			node = node->getBestChild();
			state.place(node->pos);
		}
		return node;
	}

	Node* treePolicy(board& state) {
		//selection
		Node* cur = this->traverse(state);

		//expansion
		if(cur->expand(state)){
			cur->expanded_count++;
			cur = cur->getBestChild();
			state.place( cur->pos );
			return cur;
		}
		// std::cout<<"expand failed"<<std::endl;
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
		// backpropagate the result till the root
		Node* node = this;
		while (node != nullptr) {
			node->visits++;
			node->wins += static_cast<size_t>(winner == node->who);
			node = node->parent;
		}
	}

	bool is_fully_expanded() const {
		return is_expanded && expanded_count == children.size();
	}

	Node* parent = nullptr;
	int visits = 0;
	int wins = 0;
	long unsigned int expanded_count = 0;
	size_t who;
	board::point pos;
	std::vector<Node*> children;
	bool is_leaf = false, is_expanded = false;
};

/**
 * MCTS player for both side
 * use MCTS to find the best move
 */
class mcts_player : public random_agent {
   public:
    mcts_player(const std::string& args = "")
        : random_agent("name=random role=unknown " + args),
          who(board::empty) {
        if (name().find_first_of("[]():; ") != std::string::npos)
            throw std::invalid_argument("invalid name: " + name());
        if (role() == "black") who = board::black;
        if (role() == "white") who = board::white;
        if (who == board::empty)
            throw std::invalid_argument("invalid role: " + role());
		if (meta.find("T") != meta.end())
			T = int(meta["T"]);
    }
	
	// just for test
	void print_tree( Node* root, int depth ){
		if( root == nullptr ) return;
		for( int i = 0; i < depth; i++ ) std::cout<<"  ";
		std::cout<<root->pos<<"\t"<<root->wins<<"/"<<root->visits<<"\t"<<root->expanded_count<<"/"<<root->children.size()<<std::endl;
		for( auto& child : root->children ){
			print_tree( child, depth+1 );
		}
	}

    virtual action take_action(const board& state) {
		
		// std::cout<<state<<std::endl;
		
		Node* root = new Node( 3u-who, board::point(-1, -1) );
		for( int i = 0; i < T ; i++ ) {

			board after = board(state);

			// find the best node to expand
			root->expand(after);
			Node* expand_node = root->treePolicy( after );
			// std::cout<<expand_node->pos.x<<" "<<expand_node->pos.y<<std::endl;

			// random run to add node and get reward
			size_t winner = expand_node->defaultPolicy( after );

			// update all passing nodes with reward
			expand_node->backPropagate( winner );

			// print_tree(root, 0);
		}


		// get the best child
		Node* best_child = root->getBestChild();

		// for test
		std::cout<<"-----------------"<<std::endl;
		std::cout<<state<<std::endl;
		print_tree(root, 0);
		if( best_child == nullptr ){
			std::cout<<"best child is null"<<std::endl;
		}
		else{
			std::cout<<"best child : "<<best_child->pos.x<<","<<best_child->pos.y<<std::endl;
		}

		// return action() if best child is null
		if( best_child == nullptr ) return action();

		action::place move = action::place(best_child->pos.x, best_child->pos.y, who);


        return move;
    }

   private:
    // std::vector<action::place> space;
    board::piece_type who;
	int T;
};

