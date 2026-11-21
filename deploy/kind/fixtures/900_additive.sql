CREATE TABLE flipkiln_acceptance_upgrade (
    id boolean PRIMARY KEY CHECK (id)
);
INSERT INTO flipkiln_acceptance_upgrade (id) VALUES (true);
