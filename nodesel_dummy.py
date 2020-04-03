from pyscipopt.scip import Model, Nodesel

class Default(Nodesel):
    def nodeselect(self): 
        result = self.model.executeNodeSel('estimate')    
    
def test_nodesel():
    m = Model()
    m.includeNodesel(Default(), "dummy_sel", "Testing a node selector.", 1073741823, 536870911)
    m.readProblem("v_simple.lp")
    m.optimize()

if __name__ == "__main__":
    test_nodesel()
